#pragma once

#include <atomic>
#include <memory>
#include <QMutex>
#include <QString>
#include <QThread>
#include <QWaitCondition>
#include "MediaBuffer.h"
#include "MediaContext.h"

class DemuxThread : public QThread {
    Q_OBJECT

public:
    explicit DemuxThread(QObject* parent = nullptr, std::shared_ptr<MediaBuffer> buffer = nullptr);
    ~DemuxThread();

    void start();
    void stop();
    void seek(int64_t seconds);

signals:
    void demuxError(const QString& error);
    void flushRequest();

protected:
    void run() override;

private:
    void processPacket();
    void performSeek();
    void cleanup();

private:
    std::shared_ptr<MediaBuffer> buffer_;
    AVFormatContext* inputCtx_;
    AVPacket* pkt_;
    int vsIndex_;
    int asIndex_;

    QMutex mutex_;
    QMutex eofMutex_;
    QWaitCondition eofWc_;

    QString initError_;
    int64_t seekSeconds_;

    std::atomic<bool> inited_;
    std::atomic<bool> eof_;
    std::atomic<bool> seeking_;
    std::atomic<bool> running_;
    std::atomic<bool> started_;
};