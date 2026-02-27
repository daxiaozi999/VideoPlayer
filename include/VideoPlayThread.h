#pragma once

#include <atomic>
#include <memory>
#include <QMutex>
#include <QString>
#include <QThread>
#include <QWaitCondition>
#include "YUVRenderer.h"
#include "MediaBuffer.h"
#include "MediaContext.h"
#include "AVSyncManager.h"

class VideoPlayThread : public QThread {
    Q_OBJECT

public:
    explicit VideoPlayThread(QObject* parent = nullptr, YUVRenderer* yuvRenderer = nullptr, std::shared_ptr<MediaBuffer> buffer = nullptr);
    ~VideoPlayThread();

    void start();
    void stop();
    void pause();
    void resume();
    void setSpeed(float speed);
    double getCurrentTime() const;

public slots:
    void onFlushRequest();
    void onUpdateAudioClock(double pts, double duration);

signals:
    void videoPlayError(const QString& error);

protected:
    void run() override;

private:
    int processFrame(AVFrame* frame);

private:
    YUVRenderer* yuvRenderer_;
    std::shared_ptr<MediaBuffer> buffer_;
    std::unique_ptr<media::AVSyncManager> avsyncManager_;

    QMutex mutex_;
    QMutex pauseMutex_;
    QWaitCondition pauseWc_;

    QString initError_;
    float speed_;

    std::atomic<bool> inited_;
    std::atomic<bool> paused_;
    std::atomic<bool> running_;
    std::atomic<bool> started_;
    std::atomic<double> currentTime_;
};