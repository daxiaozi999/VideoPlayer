#pragma once

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <memory>
#include "SDK.h"
#include "MediaBuffer.h"
#include "MediaContext.h"
#include "TempoFilter.h"

class AudioPlayThread : public QThread {
    Q_OBJECT

public:
    AudioPlayThread(QObject* parent = nullptr, std::shared_ptr<MediaBuffer> buffer = nullptr);
    ~AudioPlayThread();

    bool init();
    void start();
    void stop();
    void pause();
    void resume();
    void setSpeed(float speed);
    void setVolume(int volume);

public slots:
    void onFlushStream();

signals:
    void audioPlayError(const QString& errorMsg);
    void updateAudioClock(double pts, double duration);

protected:
    void run() override;

private:
    int processFrame(uint8_t* buffer, int bufferSize);
    int processVolume(uint8_t* buffer, int size);
    int calculateSleepTime(int remainBuffer, int processedSize, float currentSpeed);
    void cleanup();

private:
    std::shared_ptr<MediaBuffer> buffer_;
    std::unique_ptr<TempoFilter> filter_;

    SDL_AudioDeviceID deviceId_;
    SDL_AudioStream* audioStream_;
    uint8_t* audioBuffer_;
    unsigned int bufferSize_;

    int audioBaseDelay_;
    int audioFrameSize_;
    int audioBaseDuration_;
    int bytesPerSample_;
    double usPerBytes_;

    mutable QMutex mtx_;
    mutable QMutex pauseMtx_;
    QWaitCondition pauseCond_;
    std::atomic<bool> flush_;
    std::atomic<bool> running_;

    float volume_;
    float speed_;
    bool paused_;
};