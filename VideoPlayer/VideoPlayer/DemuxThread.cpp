#include "DemuxThread.h"

DemuxThread::DemuxThread(QObject* parent, std::shared_ptr<MediaBuffer> buffer)
    : QThread(parent)
    , buffer_(buffer)
    , fmtCtx_(nullptr)
    , pkt_(nullptr)
    , vSIdx_(-1)
    , aSIdx_(-1)
    , seekPos_(0)
    , seeking_(false)
    , eof_(false)
    , isLive_(false)
    , running_(false) {
}

DemuxThread::~DemuxThread() {
    stop();
}

bool DemuxThread::init() {
    QMutexLocker locker(&mtx_);

    running_.store(false);

    {
        QMutexLocker eofLocker(&eofMtx_);
        eof_ = false;
    }

    seeking_ = false;
    cleanup();

    if (!buffer_) {
        emit demuxError("MediaBuffer is null");
        return false;
    }

    fmtCtx_ = MediaContext::getInstance().getFormatContext();
    if (!fmtCtx_) {
        emit demuxError("Format context is null");
        return false;
    }

	isLive_ = MediaContext::getInstance().getStreamType() == media::StreamType::LIVE;

    if (MediaContext::getInstance().hasVideo()) {
        vSIdx_ = MediaContext::getInstance().getVideoParameters().streamIndex;
    }

    if (MediaContext::getInstance().hasAudio()) {
        aSIdx_ = MediaContext::getInstance().getAudioParameters().streamIndex;
    }

    if (vSIdx_ < 0 && aSIdx_ < 0) {
        emit demuxError("No valid video or audio stream found");
        return false;
    }

    pkt_ = av_packet_alloc();
    if (!pkt_) {
        emit demuxError("Failed to allocate AVPacket");
        return false;
    }

    return true;
}

void DemuxThread::start() {
    if (!init()) return;
    QThread::start();
}

void DemuxThread::run() {
    running_.store(true);

    while (running_.load()) {
        bool shouldSeek = false;
        bool isEof = false;

        {
            QMutexLocker locker(&mtx_);
            shouldSeek = seeking_;

            QMutexLocker eofLocker(&eofMtx_);
            isEof = eof_;
        }

        if (isEof) {
            QMutexLocker eofLocker(&eofMtx_);
            if (running_.load()) {
                eofCond_.wait(&eofMtx_, 100);
            }
            continue;
        }

        if (shouldSeek) {
            performSeek();
            continue;
        }

        int ret = av_read_frame(fmtCtx_, pkt_);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                QMutexLocker eofLocker(&eofMtx_);
                eof_ = true;
            }
            else if (ret == AVERROR(EAGAIN)) {
                msleep(10);
            }
            else {
                emit demuxError(QString("Network error: %1").arg(ret));
                return;
            }
            continue;
        }

        processPacket();
    }
}

void DemuxThread::stop() {
    running_.store(false);

    {
        QMutexLocker eofLocker(&eofMtx_);
        eof_ = false;
        eofCond_.wakeAll();
    }

    if (!wait(3000)) {
        requestInterruption();
        if (!wait(1000)) {
            terminate();
            wait(1000);
        }
    }

    cleanup();
}

void DemuxThread::seek(int64_t seconds) {
    if (!running_.load() || isLive_) {
        return;
    }

    {
        QMutexLocker locker(&mtx_);
        seekPos_ = seconds;
        seeking_ = true;
    }

    {
        QMutexLocker eofLocker(&eofMtx_);
        if (eof_) {
            eof_ = false;
            eofCond_.wakeAll();
        }
    }
}

void DemuxThread::processPacket() {
    if (!pkt_ || !buffer_) return;

    if (pkt_->stream_index != vSIdx_ && pkt_->stream_index != aSIdx_) {
        av_packet_unref(pkt_);
        return;
    }

    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        av_packet_unref(pkt_);
        return;
    }

    av_packet_move_ref(packet, pkt_);

    bool ok = false;
    if (packet->stream_index == vSIdx_) {
        ok = buffer_->enqueue<AVPacket>(packet, MediaType::VideoPacket);
    }
    else if (packet->stream_index == aSIdx_) {
        ok = buffer_->enqueue<AVPacket>(packet, MediaType::AudioPacket);
    }

    if (!ok) av_packet_free(&packet);
}

void DemuxThread::performSeek() {
    if (!fmtCtx_ || !buffer_ || isLive_) {
        QMutexLocker locker(&mtx_);
        seeking_ = false;
        return;
    }

    buffer_->abort();
    buffer_->cleanup();

    emit flushDecoder();
    emit flushStream();
    emit resetClocks();

    int64_t seekTime;
    {
        QMutexLocker locker(&mtx_);
        seekTime = seekPos_;
        seeking_ = false;
    }

    int64_t timestamp = seekTime * AV_TIME_BASE;
    int ret = av_seek_frame(fmtCtx_, -1, timestamp, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        emit demuxError(QString("Seek error: %1").arg(ret));
	}

    buffer_->resume();
}

void DemuxThread::cleanup() {
    if (pkt_) {
        av_packet_free(&pkt_);
        pkt_ = nullptr;
    }

    fmtCtx_ = nullptr;
    vSIdx_ = -1;
    aSIdx_ = -1;
}