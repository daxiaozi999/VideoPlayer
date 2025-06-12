#include "VideoPlayThread.h"
#include <Logger.h>

VideoPlayThread::VideoPlayThread(QObject* parent, std::shared_ptr<MediaBuffer> buffer,
    YUVRenderer* yuvRenderer)
    : QThread(parent)
    , buffer_(buffer)
    , syncManager_(nullptr)
    , yuvRenderer_(yuvRenderer)
    , curTime_(0.0)
    , speed_(1.0f)
    , paused_(false)
    , running_(false) {
    Logger::getInstance().setOutputToFile();
}

VideoPlayThread::~VideoPlayThread() {
    stop();
}

bool VideoPlayThread::init() {
    running_.store(false);

    {
        QMutexLocker locker(&pauseMtx_);
        paused_ = false;
    }

    curTime_.store(0.0);

    if (!buffer_) {
        emit videoPlayError("MediaBuffer is null");
        return false;
    }

    if (!yuvRenderer_) {
        emit videoPlayError("YUVRenderer is null");
        return false;
    }

    if (!syncManager_) {
        syncManager_ = std::make_shared<AVSyncManager>();
        if (!syncManager_) {
            emit videoPlayError("Failed to create AVSyncManager");
            return false;
        }
    }

    return true;
}

void VideoPlayThread::start() {
    if (!init()) return;
    QThread::start();
}

void VideoPlayThread::stop() {
    running_.store(false);

    {
        QMutexLocker locker(&pauseMtx_);
        paused_ = false;
        pauseCond_.wakeAll();
    }

    if (!wait(3000)) {
        requestInterruption();
        if (!wait(1000)) {
            terminate();
            wait(1000);
        }
    }
}

void VideoPlayThread::pause() {
    QMutexLocker locker(&pauseMtx_);
    if (!paused_) {
        if (syncManager_) {
            syncManager_->pause();
        }
        paused_ = true;
    }
}

void VideoPlayThread::resume() {
    QMutexLocker locker(&pauseMtx_);
    if (paused_) {
        if (syncManager_) {
            syncManager_->resume();
        }
        paused_ = false;
        pauseCond_.wakeAll();
    }
}

void VideoPlayThread::setSpeed(float speed) {
    QMutexLocker locker(&mtx_);
    if (std::fabs(speed_ - speed) < 0.01f) return;

    speed_ = speed;
    if (syncManager_) {
        syncManager_->setSpeed(static_cast<double>(speed));
    }
}

double VideoPlayThread::getCurrentTime() const {
    return curTime_;
}

void VideoPlayThread::onResetClock() {
    QMutexLocker locker(&mtx_);
    if (syncManager_) {
        syncManager_->reset();
    }
}

void VideoPlayThread::onUpdateAudioClock(double pts, double duration) {
    if (syncManager_) {
        syncManager_->updateAudioClock(pts, duration);
    }
}

void VideoPlayThread::run() {
    running_.store(true);

    while (running_.load()) {
        bool shouldPause;
        {
            QMutexLocker locker(&pauseMtx_);
            shouldPause = paused_;
        }

        if (shouldPause) {
            QMutexLocker lock(&pauseMtx_);
            if (running_.load()) {
                pauseCond_.wait(&pauseMtx_, 100);
            }
            continue;
        }

        AVFrame* frame = buffer_->dequeue<AVFrame>(MediaType::VideoFrame);
        if (!frame) {
            msleep(5);
            continue;
        }

        int delay = processFrame(frame);

        if (delay > 0) {
            msleep(delay);
        }
        
        av_frame_free(&frame);
    }
}

int VideoPlayThread::processFrame(AVFrame* frame) {
    if (!frame || !yuvRenderer_) {
        return 0;
    }

    static AVRational timebase = MediaContext::getInstance().getVideoParameters().timebase;
    double pts = frame->pts * av_q2d(timebase);
    double duration = frame->duration * av_q2d(timebase);
    curTime_.store(pts);

    int delay = performSync(pts, duration);

    yuvRenderer_->updateYUVFrame(frame->data[0], frame->data[1], frame->data[2],
                                 frame->width, frame->height, frame->linesize[0],
                                 frame->linesize[1], frame->linesize[2]);

    return delay;
}

int VideoPlayThread::performSync(double pts, double duration) {
    if (!syncManager_) {
        return static_cast<int>(duration * 1000);
    }

    int delay = syncManager_->calculateVideoDelay(pts, duration);

    return delay;
}