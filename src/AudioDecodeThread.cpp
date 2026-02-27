#include "AudioDecodeThread.h"

AudioDecodeThread::AudioDecodeThread(QObject* parent, std::shared_ptr<MediaBuffer> buffer)
    : QThread(parent)
    , buffer_(buffer)
    , decCtx_(nullptr)
    , swrCtx_(nullptr)
    , decFrm_(nullptr)
    , pcmFrm_(nullptr)
    , initError_("")
    , inited_(false)
    , flush_(false)
    , running_(false)
    , started_(false) {

    do {
        if (!buffer) {
            initError_ = "Media buffer is NULL";
            break;
        }

        decCtx_ = MCTX()->mediaDecoder()->audioDecoder();
        if (!decCtx_) {
            initError_ = "Audio decoder context is NULL";
            break;
        }

        swrCtx_ = MCTX()->mediaResampler()->swrContext();
        if (!swrCtx_) {
            initError_ = "Swr context is NULL";
            break;
        }

        decFrm_ = av_frame_alloc();
        pcmFrm_ = av_frame_alloc();
        if (!decFrm_ || !pcmFrm_) {
            initError_ = "AVFrame alloc failed";
            break;
        }

        inited_.store(true);

    } while (0);

    if (!inited_.load()) {
        cleanup();
    }
}

AudioDecodeThread::~AudioDecodeThread() {
    stop();
    cleanup();
}

void AudioDecodeThread::start() {
    if (started_.exchange(true)) {
        return;
    }

    if (!inited_.load()) {
        emit audioDecodeError(initError_);
        return;
    }

    QThread::start();
}

void AudioDecodeThread::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    flush_.store(false);
    requestInterruption();

    if (!wait(3000)) {
        terminate();
        wait(1000);
    }

    started_.store(false);
}

void AudioDecodeThread::onFlushRequest() {
    if (!running_.load()) {
        return;
    }

    flush_.store(true);
}

void AudioDecodeThread::run() {
    running_.store(true);

    while (running_.load() && !isInterruptionRequested()) {
        if (flush_.exchange(false)) {
            avcodec_flush_buffers(decCtx_);
            continue;
        }

        AVPacket* packet = reinterpret_cast<AVPacket*>(buffer_->dequeue<media::AUDIO, media::DEMUXING>());
        if (!packet) {
            msleep(1);
            continue;
        }

        int ret = avcodec_send_packet(decCtx_, packet);
        av_packet_free(&packet);

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                flushDecoder();
            }
            else if (ret == AVERROR(EAGAIN)) {
                msleep(1);
            }
            else {
                emit audioDecodeError("Audio decode thread send packet failed");
                break;
            }
            continue;
        }

        bool innerError = false;
        while (running_.load() && !innerError) {
            av_frame_unref(decFrm_);
            ret = avcodec_receive_frame(decCtx_, decFrm_);
            if (ret == 0) {
                processFrame();
            }
            else if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                break;
            }
            else {
                emit audioDecodeError("Audio decode thread receive frame failed");
                innerError = true;
                break;
            }
        }

        if (innerError) {
            break;
        }
    }

    running_.store(false);
}

void AudioDecodeThread::processFrame() {
    if (!decFrm_ || !pcmFrm_ || !swrCtx_ || !decCtx_ || !buffer_) {
        return;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        return;
    }

    if (decFrm_->pts == AV_NOPTS_VALUE) {
        decFrm_->pts = decFrm_->best_effort_timestamp;
    }
    decFrm_->time_base = decCtx_->time_base;

    if (decCtx_->ch_layout.nb_channels == MediaContext::TARGET_CHANNEL_LAYOUT.nb_channels &&
        decCtx_->sample_fmt == MediaContext::TARGET_SAMPLE_FORMAT) {
        av_frame_move_ref(frame, decFrm_);
    }
    else {
        av_frame_unref(pcmFrm_);
        pcmFrm_->sample_rate = decCtx_->sample_rate;
        pcmFrm_->nb_samples = decFrm_->nb_samples;
        pcmFrm_->format = MediaContext::TARGET_SAMPLE_FORMAT;
        av_channel_layout_copy(&pcmFrm_->ch_layout, &MediaContext::TARGET_CHANNEL_LAYOUT);

        if (av_frame_get_buffer(pcmFrm_, 0) < 0) {
            av_frame_free(&frame);
            return;
        }

        int ret = swr_convert_frame(swrCtx_, pcmFrm_, decFrm_);
        if (ret < 0) {
            av_frame_free(&frame);
            return;
        }

        pcmFrm_->pts = decFrm_->pts;
        pcmFrm_->pkt_dts = decFrm_->pts;
        pcmFrm_->duration = decFrm_->duration;
        pcmFrm_->time_base = decFrm_->time_base;

        av_frame_move_ref(frame, pcmFrm_);
    }

    bool ok = buffer_->enqueue<AVFrame, media::AUDIO, media::DECODING>(frame);
    if (!ok) {
        av_frame_free(&frame);
    }
}

void AudioDecodeThread::flushDecoder() {
    if (!decFrm_ || !decCtx_) {
        return;
    }

    int ret = avcodec_send_packet(decCtx_, nullptr);
    if (ret < 0 && ret != AVERROR_EOF) {
        return;
    }

    while (running_.load()) {
        av_frame_unref(decFrm_);
        ret = avcodec_receive_frame(decCtx_, decFrm_);
        if (ret == 0) {
            processFrame();
        }
        else {
            break;
        }
    }
}

void AudioDecodeThread::cleanup() {
    if (decFrm_) {
        av_frame_free(&decFrm_);
        decFrm_ = nullptr;
    }

    if (pcmFrm_) {
        av_frame_free(&pcmFrm_);
        pcmFrm_ = nullptr;
    }
}