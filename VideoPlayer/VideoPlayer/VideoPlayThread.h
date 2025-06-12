#pragma once

#include <QObject>
#include <QWidget>
#include <QImage>
#include <QSize>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <memory>
#include "SDK.h"
#include "MediaBuffer.h"
#include "MediaContext.h"
#include "MediaResampler.h"
#include "AVSyncManager.h"
#include "YUVRenderer.h"

class VideoPlayThread : public QThread {
    Q_OBJECT

public:
    VideoPlayThread(QObject* parent = nullptr, std::shared_ptr<MediaBuffer> buffer = nullptr,
                    YUVRenderer* yuvRenderer = nullptr);
    ~VideoPlayThread();

    bool init();
    void start();
    void stop();
    void pause();
    void resume();
    void setSpeed(float speed);
    double getCurrentTime() const;

public slots:
    void onResetClock();
    void onUpdateAudioClock(double pts, double duration);

signals:
    void videoPlayError(const QString& errorMsg);

protected:
    void run() override;

private:
    int processFrame(AVFrame* frame);
    int performSync(double pts, double duration);

private:
    std::shared_ptr<MediaBuffer> buffer_;
    std::shared_ptr<AVSyncManager> syncManager_;
    YUVRenderer* yuvRenderer_;
    mutable QMutex mtx_;
    mutable QMutex pauseMtx_;
    QWaitCondition pauseCond_;
    float speed_;
    bool paused_;
    std::atomic<double> curTime_;
    std::atomic<bool> running_;
};