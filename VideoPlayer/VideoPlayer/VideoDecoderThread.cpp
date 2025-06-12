#include "VideoDecoderThread.h"

VideoDecoderThread::VideoDecoderThread(QObject* parent, std::shared_ptr<MediaBuffer> buffer)
    : QThread(parent)
    , buffer_(buffer)
    , decCtx_(nullptr)
    , swsCtx_(nullptr)
    , decFrm_(nullptr)
    , yuvFrm_(nullptr)
    , flush_(false)
    , running_(false) {
}

VideoDecoderThread::~VideoDecoderThread() {
    stop();
}

bool VideoDecoderThread::init() {
    running_.store(false);
    flush_.store(false);
    cleanup();

    if (!buffer_) {
        emit videoDecodeError("MediaBuffer is null");
        return false;
    }

    decCtx_ = MediaContext::getInstance().getVideoDecoder();
    swsCtx_ = MediaContext::getInstance().getSwsContext();
    if (!decCtx_ || !swsCtx_) {
        emit videoDecodeError("Failed to get decoder or sws context");
        return false;
    }

    decFrm_ = av_frame_alloc();
    yuvFrm_ = av_frame_alloc();
    if (!decFrm_ || !yuvFrm_) {
        emit videoDecodeError("Failed to allocate frames");
        return false;
    }

    return true;
}

void VideoDecoderThread::start() {
    if (!init()) return;
    QThread::start();
}

void VideoDecoderThread::stop() {
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

void VideoDecoderThread::onFlushRequest() {
    flush_.store(true);
}

void VideoDecoderThread::run() {
    running_.store(true);

    while (running_.load()) {
        if (flush_.exchange(false)) {
            if (decCtx_) {
                avcodec_flush_buffers(decCtx_);
            }
            continue;
        }

        AVPacket* packet = buffer_->dequeue<AVPacket>(MediaType::VideoPacket);
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
                emit videoDecodeError(QString("Error sending packet: %1").arg(ret));
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
                emit videoDecodeError(QString("Error receiving frame: %1").arg(ret));
                av_packet_free(&packet);
                return;
            }
        }

        av_packet_free(&packet);
    }
}

void VideoDecoderThread::processFrame() {
    if (!decFrm_ || !yuvFrm_ || !swsCtx_ || !running_.load()) {
        return;
    }

    av_frame_unref(yuvFrm_);

    yuvFrm_->width = decCtx_->width;
    yuvFrm_->height = decCtx_->height;
    yuvFrm_->format = MediaContext::DEST_PIX_FMT;

    int ret = av_frame_get_buffer(yuvFrm_, 0);
    if (ret < 0) {
        return;
    }

    if (decFrm_->pts == AV_NOPTS_VALUE) {
        decFrm_->pts = decFrm_->best_effort_timestamp;
    }

    ret = sws_scale(swsCtx_, decFrm_->data, decFrm_->linesize, 0,
        decFrm_->height, yuvFrm_->data, yuvFrm_->linesize);
    if (ret < 0) {
        return;
    }

    yuvFrm_->pts        = decFrm_->pts;
    yuvFrm_->time_base  = decCtx_->time_base;
    yuvFrm_->duration   = decFrm_->duration;
    yuvFrm_->pkt_dts    = decFrm_->pkt_dts;

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        return;
    }

    ret = av_frame_ref(frame, yuvFrm_);
    if (ret < 0) {
        av_frame_free(&frame);
        return;
    }

    bool ok = buffer_->enqueue<AVFrame>(frame, MediaType::VideoFrame);
    if (!ok) {
        av_frame_free(&frame);
    }
}

void VideoDecoderThread::flushDecoder() {
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

void VideoDecoderThread::cleanup() {
    if (decFrm_) {
        av_frame_free(&decFrm_);
        decFrm_ = nullptr;
    }

    if (yuvFrm_) {
        av_frame_free(&yuvFrm_);
        yuvFrm_ = nullptr;
    }

    decCtx_ = nullptr;
    swsCtx_ = nullptr;
}