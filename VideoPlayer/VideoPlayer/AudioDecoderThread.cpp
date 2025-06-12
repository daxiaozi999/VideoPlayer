#include "AudioDecoderThread.h"

AudioDecoderThread::AudioDecoderThread(QObject* parent, std::shared_ptr<MediaBuffer> buffer)
    : QThread(parent)
    , buffer_(buffer)
    , decCtx_(nullptr)
    , swrCtx_(nullptr)
    , decFrm_(nullptr)
    , pcmFrm_(nullptr)
    , flush_(false)
    , running_(false) {
}

AudioDecoderThread::~AudioDecoderThread() {
    stop();
}

bool AudioDecoderThread::init() {
    flush_.store(false);
    running_.store(false);
    cleanup();

    if (!buffer_) {
        emit audioDecodeError("MediaBuffer is null");
        return false;
    }

    decCtx_ = MediaContext::getInstance().getAudioDecoder();
    swrCtx_ = MediaContext::getInstance().getSwrContext();
    if (!decCtx_ || !swrCtx_) {
        emit audioDecodeError("Failed to get audio decoder or swr context");
        return false;
    }

    decFrm_ = av_frame_alloc();
    pcmFrm_ = av_frame_alloc();
    if (!decFrm_ || !pcmFrm_) {
        emit audioDecodeError("Failed to allocate audio frames");
        return false;
    }

    return true;
}

void AudioDecoderThread::start() {
    if (!init()) return;
    QThread::start();
}

void AudioDecoderThread::stop() {
    running_.store(false);
    flush_.store(false);

    if (!wait(3000)) {
        requestInterruption();
        if (!wait(1000)) {
            terminate();
            wait(1000);
        }
    }

    cleanup();
}

void AudioDecoderThread::onFlushRequest() {
    flush_.store(true);
}

void AudioDecoderThread::run() {
    running_.store(true);

    while (running_.load()) {
        if (flush_.exchange(false)) {
            if (decCtx_) {
                avcodec_flush_buffers(decCtx_);
            }
            continue;
        }

        AVPacket* packet = buffer_->dequeue<AVPacket>(MediaType::AudioPacket);
        if (!packet) {
            msleep(5);
            continue;
        }

        int ret = avcodec_send_packet(decCtx_, packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                flushDecoder();
            }
            else if (ret == AVERROR(EAGAIN)) {
				// nothing
            }
            else {
                emit audioDecodeError(QString("Failed to send packet to decoder: %1").arg(ret));
                av_packet_free(&packet);
                return;
            }
        }

        while (running_.load()) {
            av_frame_unref(decFrm_);
            ret = avcodec_receive_frame(decCtx_, decFrm_);
            if (ret == 0) {
                processFrame();
            }
            else if (ret == AVERROR_EOF) {
                break;
            }
            else if (ret == AVERROR(EAGAIN)) {
                break;
            }
            else {
                emit audioDecodeError(QString("Failed to receive frame from decoder: %1").arg(ret));
                av_packet_free(&packet);
                return;
            }
        }

        av_packet_free(&packet);
    }
}

void AudioDecoderThread::processFrame() {
    if (!decFrm_ || !pcmFrm_ || !swrCtx_ || !running_.load()) return;

    av_frame_unref(pcmFrm_);

    pcmFrm_->sample_rate    = decFrm_->sample_rate;
    pcmFrm_->nb_samples     = decFrm_->nb_samples;
    pcmFrm_->format         = MediaContext::DEST_SAMPLE_FMT;
    av_channel_layout_copy(&pcmFrm_->ch_layout, &MediaContext::DEST_CHANNEL_LAYOUT);

    int ret = av_frame_get_buffer(pcmFrm_, 0);
    if (ret < 0) {
        return;
    }

    AVRational tb = { 1, decFrm_->sample_rate };

    if (decFrm_->best_effort_timestamp != AV_NOPTS_VALUE) {
        decFrm_->pts = av_rescale_q(decFrm_->best_effort_timestamp, decCtx_->time_base, tb);
    }
    else if (decFrm_->pts != AV_NOPTS_VALUE) {
        decFrm_->pts = av_rescale_q(decFrm_->pts, decCtx_->time_base, tb);
    }

    ret = swr_convert_frame(swrCtx_, pcmFrm_, decFrm_);
    if (ret < 0) {
        return;
    }

    pcmFrm_->pts        = decFrm_->pts;
    pcmFrm_->time_base  = tb;
    pcmFrm_->duration   = pcmFrm_->nb_samples;
    pcmFrm_->pkt_dts    = decFrm_->pkt_dts;

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        return;
    }

    ret = av_frame_ref(frame, pcmFrm_);
    if (ret < 0) {
        av_frame_free(&frame);
        return;
    }

    bool ok = buffer_->enqueue<AVFrame>(frame, MediaType::AudioFrame);
    if (!ok) {
        av_frame_free(&frame);
    }
}

void AudioDecoderThread::flushDecoder() {
    if (!decCtx_ || !running_.load()) return;

    int ret = avcodec_send_packet(decCtx_, nullptr);
    if (ret < 0) {
        return;
    }

    while (running_.load()) {
        av_frame_unref(decFrm_);
        ret = avcodec_receive_frame(decCtx_, decFrm_);
        if (ret < 0) {
            break;
        }
        processFrame();
    }
}

void AudioDecoderThread::cleanup() {
    if (decFrm_) {
        av_frame_free(&decFrm_);
        decFrm_ = nullptr;
    }

    if (pcmFrm_) {
        av_frame_free(&pcmFrm_);
        pcmFrm_ = nullptr;
    }

    decCtx_ = nullptr;
    swrCtx_ = nullptr;
}