#pragma once

#include <atomic>
#include <memory>
#include <QMutex>
#include <QString>
#include <QThread>
#include "SDL3.h"
#include "TempoFilter.h"
#include "MediaBuffer.h"
#include "MediaContext.h"

class AudioPlayThread : public QThread {
    Q_OBJECT

public:
    explicit AudioPlayThread(QObject* parent = nullptr, std::shared_ptr<MediaBuffer> buffer = nullptr);
    ~AudioPlayThread();

    void start();
    void stop();
    void pause();
    void resume();
    void setSpeed(float speed);
    void setVolume(int volume);
    double getCurrentTime() const;

public slots:
    void onFlushStream();

signals:
    void audioPlayError(const QString& error);
    void updateAudioClock(double pts, double duration);

protected:
    void run() override;

private:
    int processFrame(uint8_t* buffer, int size);
    int processVolume(uint8_t* buffer, int size);
    static void SDLCALL audioStreamCallback(void* userdata, SDL_AudioStream* stream, int additional, int total);
    void cleanup();

private:
    std::shared_ptr<MediaBuffer> buffer_;
    std::unique_ptr<media::TempoFilter> filter_;

    SDL_AudioStream* SDLAudioStream_;
    uint8_t* SDLAudioBuffer_;
    size_t SDLAudioBufferSize_;

    QMutex mutex_;
    QString initError_;

    float speed_;
    float volume_;

    std::atomic<bool> inited_;
    std::atomic<bool> flush_;
    std::atomic<bool> paused_;
    std::atomic<bool> running_;
    std::atomic<bool> started_;
    std::atomic<double> currentTime_;
};