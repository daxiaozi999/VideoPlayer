#include "VideoDecodeThread.h"

VideoDecodeThread::VideoDecodeThread(QObject* parent, std::shared_ptr<MediaBuffer> buffer)
    : QThread(parent)
    , buffer_(buffer)
    , decCtx_(nullptr)
    , swsCtx_(nullptr)
    , decFrm_(nullptr)
    , yuvFrm_(nullptr)
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

        decCtx_ = MCTX()->mediaDecoder()->videoDecoder();
        if (!decCtx_) {
            initError_ = "Video decoder context is NULL";
            break;
        }

        swsCtx_ = MCTX()->mediaResampler()->swsContext();
        if (!swsCtx_) {
            initError_ = "Sws context is NULL";
            break;
        }

        decFrm_ = av_frame_alloc();
        yuvFrm_ = av_frame_alloc();
        if (!decFrm_ || !yuvFrm_) {
            initError_ = "AVFrame alloc failed";
            break;
        }

        inited_.store(true);

    } while (0);

    if (!inited_.load()) {
        cleanup();
    }
}

VideoDecodeThread::~VideoDecodeThread() {
    stop();
    cleanup();
}

void VideoDecodeThread::start() {
    if (started_.exchange(true)) {
        return;
    }

    if (!inited_.load()) {
        emit videoDecodeError(initError_);
        return;
    }

    QThread::start();
}

void VideoDecodeThread::stop() {
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

void VideoDecodeThread::onFlushRequest() {
    if (!running_.load()) {
        return;
    }

    flush_.store(true);
}

void VideoDecodeThread::run() {
    running_.store(true);

    while (running_.load() && !isInterruptionRequested()) {
        if (flush_.exchange(false)) {
            if (decCtx_) {
                avcodec_flush_buffers(decCtx_);
            }
            continue;
        }

        AVPacket* packet = reinterpret_cast<AVPacket*>(buffer_->dequeue<media::VIDEO, media::DEMUXING>());
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
                emit videoDecodeError("Video decode thread send packet failed");
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
                emit videoDecodeError("Video decode thread receive frame failed");
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

void VideoDecodeThread::processFrame() {
    if (!decFrm_ || !yuvFrm_ || !swsCtx_ || !decCtx_ || !buffer_) {
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

    if (decCtx_->pix_fmt == MediaContext::TARGET_PIXEL_FORMAT) {
        av_frame_move_ref(frame, decFrm_);
    }
    else {
        av_frame_unref(yuvFrm_);
        yuvFrm_->width = decCtx_->width;
        yuvFrm_->height = decCtx_->height;
        yuvFrm_->format = MediaContext::TARGET_PIXEL_FORMAT;

        if (av_frame_get_buffer(yuvFrm_, 0) < 0) {
            av_frame_free(&frame);
            return;
        }

        int ret = sws_scale(swsCtx_, decFrm_->data, decFrm_->linesize, 0,
                            decFrm_->height, yuvFrm_->data, yuvFrm_->linesize);
        if (ret < 0) {
            av_frame_free(&frame);
            return;
        }

        yuvFrm_->pts = decFrm_->pts;
        yuvFrm_->pkt_dts = decFrm_->pkt_dts;
        yuvFrm_->duration = decFrm_->duration;
        yuvFrm_->time_base = decFrm_->time_base;

        av_frame_move_ref(frame, yuvFrm_);
    }

    bool ok = buffer_->enqueue<AVFrame, media::VIDEO, media::DECODING>(frame);
    if (!ok) {
        av_frame_free(&frame);
    }
}

void VideoDecodeThread::flushDecoder() {
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

void VideoDecodeThread::cleanup() {
    if (decFrm_) {
        av_frame_free(&decFrm_);
        decFrm_ = nullptr;
    }

    if (yuvFrm_) {
        av_frame_free(&yuvFrm_);
        yuvFrm_ = nullptr;
    }
}