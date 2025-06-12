#pragma once

#include <QObject>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <atomic>
#include <memory>
#include "MediaBuffer.h"
#include "MediaContext.h"
#include "SDK.h"

class DemuxThread : public QThread {
	Q_OBJECT

public:
    DemuxThread(QObject* parent = nullptr, std::shared_ptr<MediaBuffer> buffer = nullptr);
    ~DemuxThread();

    bool init();
    void start();
    void stop();
    void seek(int64_t seconds);

signals:
    void demuxError(const QString& errorMsg);
    void flushDecoder();
    void resetClocks();
    void flushStream();

protected:
    void run() override;

private:
    void processPacket();
    void performSeek();
    void cleanup();
    

private:
    std::shared_ptr<MediaBuffer> buffer_;
    AVFormatContext* fmtCtx_;
    AVPacket* pkt_;
    int vSIdx_;
    int aSIdx_;
    mutable QMutex mtx_;
    mutable QMutex eofMtx_;
    QWaitCondition eofCond_;
    int64_t seekPos_;
    bool seeking_;
    bool eof_;
    bool isLive_;
    std::atomic<bool> running_;
};

