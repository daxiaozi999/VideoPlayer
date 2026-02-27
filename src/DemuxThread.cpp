#include "DemuxThread.h"

DemuxThread::DemuxThread(QObject* parent, std::shared_ptr<MediaBuffer> buffer)
    : QThread(parent)
    , buffer_(buffer)
    , inputCtx_(nullptr)
    , pkt_(nullptr)
    , vsIndex_(-1)
    , asIndex_(-1)
    , initError_("")
    , seekSeconds_(0)
    , inited_(false)
    , eof_(false)
    , seeking_(false)
    , running_(false)
    , started_(false) {

    do {
        if (!buffer) {
            initError_ = "Media buffer is NULL";
            break;
        }

        inputCtx_ = MCTX()->mediaInput()->inputContext();
        if (!inputCtx_) {
            initError_ = "Input context is NULL";
            break;
        }

        if (MCTX()->mediaInput()->hasVideoStream()) {
            vsIndex_ = MCTX()->mediaInput()->videoParams().index;
        }
        if (MCTX()->mediaInput()->hasAudioStream()) {
            asIndex_ = MCTX()->mediaInput()->audioParams().index;
        }

        if (vsIndex_ < 0 && asIndex_ < 0) {
            initError_ = "Not found video and audio";
            break;
        }

        pkt_ = av_packet_alloc();
        if (!pkt_) {
            initError_ = "AVPacket alloc failed";
            break;
        }

        inited_.store(true);

    } while (0);

    if (!inited_.load()) {
        cleanup();
    }
}

DemuxThread::~DemuxThread() {
    stop();
    cleanup();
}

void DemuxThread::start() {
    if (started_.exchange(true)) {
        return;
    }

    if (!inited_.load()) {
        emit demuxError(initError_);
        return;
    }

    QThread::start();
}

void DemuxThread::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    seeking_.store(false);
    requestInterruption();

    eof_.store(false);
    {
        QMutexLocker locker(&eofMutex_);
        eofWc_.wakeAll();
    }

    if (!wait(3000)) {
        terminate();
        wait(1000);
    }

    started_.store(false);
}

void DemuxThread::seek(int64_t seconds) {
    if (!running_.load()) {
        return;
    }

    {
        QMutexLocker locker(&mutex_);
        seekSeconds_ = seconds;
    }
    seeking_.store(true);

    eof_.store(false);
    {
        QMutexLocker locker(&eofMutex_);
        eofWc_.wakeAll();
    }
}

void DemuxThread::run() {
    running_.store(true);

    while (running_.load() && !isInterruptionRequested()) {
        if (eof_.load()) {
            QMutexLocker locker(&eofMutex_);
            if (running_.load() && !seeking_.load()) {
                eofWc_.wait(&eofMutex_, 50);
            }
            continue;
        }

        if (seeking_.exchange(false)) {
            performSeek();
            continue;
        }

        av_packet_unref(pkt_);
        int ret = av_read_frame(inputCtx_, pkt_);

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                eof_.store(true);
            }
            else if (ret == AVERROR(EAGAIN)) {
                msleep(1);
            }
            else {
                emit demuxError("Demux thread read frame failed");
                break;
            }
            continue;
        }

        processPacket();
    }

    running_.store(false);
}

void DemuxThread::processPacket() {
    if (!pkt_ || !buffer_) {
        return;
    }

    if (pkt_->stream_index != vsIndex_ && pkt_->stream_index != asIndex_) {
        return;
    }

    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        return;
    }

    av_packet_move_ref(packet, pkt_);

    bool ok = false;
    if (packet->stream_index == vsIndex_) {
        ok = buffer_->enqueue<AVPacket, media::VIDEO, media::DEMUXING>(packet);
    }
    else if (packet->stream_index == asIndex_) {
        ok = buffer_->enqueue<AVPacket, media::AUDIO, media::DEMUXING>(packet);
    }

    if (!ok) {
        av_packet_free(&packet);
    }
}

void DemuxThread::performSeek() {
    if (!inputCtx_ || !buffer_) {
        return;
    }

    int64_t pos;
    {
        QMutexLocker locker(&mutex_);
        pos = seekSeconds_;
    }

    buffer_->lock();
    buffer_->clear();

    int64_t timestamp = pos * AV_TIME_BASE;
    int ret = av_seek_frame(inputCtx_, -1, timestamp, AVSEEK_FLAG_BACKWARD);

    if (ret < 0) {
        emit demuxError("Demux thread seek failed");
    }
    else {
        emit flushRequest();
    }

    buffer_->unlock();
}

void DemuxThread::cleanup() {
    if (pkt_) {
        av_packet_free(&pkt_);
        pkt_ = nullptr;
    }
}