#pragma once

#include <atomic>
#include <memory>
#include <QString>
#include <QThread>
#include "MediaBuffer.h"
#include "MediaContext.h"

class VideoDecodeThread : public QThread {
    Q_OBJECT

public:
    explicit VideoDecodeThread(QObject* parent = nullptr, std::shared_ptr<MediaBuffer> buffer = nullptr);
    ~VideoDecodeThread();

    void start();
    void stop();

public slots:
    void onFlushRequest();

signals:
    void videoDecodeError(const QString& error);

protected:
    void run() override;

private:
    void processFrame();
    void flushDecoder();
    void cleanup();

private:
    std::shared_ptr<MediaBuffer> buffer_;
    AVCodecContext* decCtx_;
    SwsContext* swsCtx_;
    AVFrame* decFrm_;
    AVFrame* yuvFrm_;

    QString initError_;

    std::atomic<bool> inited_;
    std::atomic<bool> flush_;
    std::atomic<bool> running_;
    std::atomic<bool> started_;
};