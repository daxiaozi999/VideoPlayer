#include "VideoPlayThread.h"

VideoPlayThread::VideoPlayThread(QObject* parent, YUVRenderer* yuvRenderer, std::shared_ptr<MediaBuffer> buffer)
    : QThread(parent)
    , yuvRenderer_(yuvRenderer)
    , buffer_(buffer)
    , avsyncManager_(nullptr)
    , initError_("")
    , speed_(1.0f)
    , inited_(false)
    , paused_(false)
    , running_(false)
    , started_(false)
    , currentTime_(0.0) {

    do {
        if (!yuvRenderer_) {
            initError_ = "YUVRenderer is NULL";
            break;
        }

        if (!buffer_) {
            initError_ = "Media Buffer is NULL";
            break;
        }

        const media::VideoParams& vp = MCTX()->mediaInput()->videoParams();
        const media::AudioParams& ap = MCTX()->mediaInput()->audioParams();

        double vd = av_q2d(av_inv_q(vp.framerate));
        double ad = av_q2d(ap.timebase) * ap.framesize;

        avsyncManager_ = std::make_unique<media::AVSyncManager>(vd, ad);
        if (!avsyncManager_) {
            initError_ = "AVSyncManager create failed";
            break;
        }

        inited_.store(true);

    } while (0);
}

VideoPlayThread::~VideoPlayThread() {
    stop();
}

void VideoPlayThread::start() {
    if (started_.exchange(true)) {
        return;
    }

    if (!inited_.load()) {
        emit videoPlayError(initError_);
        return;
    }

    QThread::start();
}

void VideoPlayThread::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    requestInterruption();

    paused_.store(false);
    {
        QMutexLocker locker(&pauseMutex_);
        pauseWc_.wakeAll();
    }

    if (!wait(3000)) {
        terminate();
        wait(1000);
    }

    started_.store(false);
}

void VideoPlayThread::pause() {
    paused_.store(true);

    if (avsyncManager_) {
        avsyncManager_->pause();
    }
}

void VideoPlayThread::resume() {
    paused_.store(false);
    {
        QMutexLocker locker(&pauseMutex_);
        pauseWc_.wakeAll();
    }

    if (avsyncManager_) {
        avsyncManager_->resume();
    }
}

void VideoPlayThread::setSpeed(float speed) {
    {
        QMutexLocker locker(&mutex_);
        if (qAbs(speed_ - speed) < 0.01f) {
            return;
        }
        speed_ = speed;
    }

    if (avsyncManager_) {
        avsyncManager_->setSpeed(static_cast<double>(speed));
    }
}

double VideoPlayThread::getCurrentTime() const {
    return currentTime_.load();
}

void VideoPlayThread::onFlushRequest() {
    if (avsyncManager_) {
        avsyncManager_->reset();
    }
}

void VideoPlayThread::onUpdateAudioClock(double pts, double duration) {
    if (avsyncManager_) {
        avsyncManager_->updateAudioClock(pts, duration);
    }
}

void VideoPlayThread::run() {
    running_.store(true);

    while (running_.load() && !isInterruptionRequested()) {
        if (paused_.load()) {
            QMutexLocker locker(&pauseMutex_);
            if (running_.load()) {
                pauseWc_.wait(&pauseMutex_, 50);
            }
            continue;
        }

        AVFrame* frame = reinterpret_cast<AVFrame*>(buffer_->dequeue<media::VIDEO, media::DECODING>());
        if (!frame) {
            msleep(1);
            continue;
        }

        int delay = processFrame(frame);
        av_frame_free(&frame);

        if (delay > 0) {
            msleep(delay);
        }
    }

    running_.store(false);
}

int VideoPlayThread::processFrame(AVFrame* frame) {
    if (!frame || !yuvRenderer_ || !avsyncManager_) {
        return 0;
    }

    double pts = frame->pts * av_q2d(frame->time_base);
    double duration = frame->duration * av_q2d(frame->time_base);

    int delay = 0;
    avsyncManager_->updateVideoClock(pts, duration, delay);

    currentTime_.store(pts);
    yuvRenderer_->updateYUVFrame(frame->data[0], frame->data[1], frame->data[2],
                                 frame->width, frame->height,
                                 frame->linesize[0], frame->linesize[1], frame->linesize[2]);

    return delay;
}