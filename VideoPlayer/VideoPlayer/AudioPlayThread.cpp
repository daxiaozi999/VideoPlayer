#include "AudioPlayThread.h"
#include <Logger.h>

AudioPlayThread::AudioPlayThread(QObject* parent, std::shared_ptr<MediaBuffer> buffer)
    : QThread(parent)
    , buffer_(buffer)
    , filter_(nullptr)
    , deviceId_(0)
    , audioStream_(nullptr)
    , audioBuffer_(nullptr)
    , bufferSize_(0)
    , audioBaseDelay_(2)
    , audioFrameSize_(0)
    , audioBaseDuration_(0)
    , bytesPerSample_(0)
    , usPerBytes_(0.0)
    , volume_(1.0f)
    , speed_(1.0f)
    , paused_(false)
    , flush_(false)
    , running_(false) {
}

AudioPlayThread::~AudioPlayThread() {
    stop();
}

bool AudioPlayThread::init() {
    running_.store(false);
    flush_.store(false);

    {
        QMutexLocker locker(&pauseMtx_);
        paused_ = false;
    }

    cleanup();

    if (!buffer_) {
        emit audioPlayError("MediaBuffer is null");
        return false;
    }

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        emit audioPlayError(QString("SDL_Init failed: %1").arg(SDL_GetError()));
        return false;
    }

    const media::AudioParameters& audioParams = MediaContext::getInstance().getAudioParameters();
    int samplerate = audioParams.samplerate;
    int channels = audioParams.chlayout.nb_channels;

    audioFrameSize_ = audioParams.framesize;
    AVRational timebase = audioParams.timebase;
    audioBaseDuration_ = static_cast<int>((av_q2d(timebase) * audioFrameSize_) * 1000);
    bytesPerSample_ = channels * sizeof(int16_t);
    usPerBytes_ = 1000000.0 / (samplerate * bytesPerSample_);

    bufferSize_ = samplerate * 3 * channels * sizeof(int16_t);
    audioBuffer_ = static_cast<uint8_t*>(av_malloc(bufferSize_));
    if (!audioBuffer_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        emit audioPlayError("Failed to allocate audio buffer");
        return false;
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = samplerate;
    spec.format = SDL_AUDIO_S16;
    spec.channels = channels;

    deviceId_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!deviceId_) {
        av_freep(&audioBuffer_);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        emit audioPlayError(QString("SDL_OpenAudioDevice failed: %1").arg(SDL_GetError()));
        return false;
    }

    audioStream_ = SDL_CreateAudioStream(&spec, &spec);
    if (!audioStream_) {
        SDL_CloseAudioDevice(deviceId_);
        av_freep(&audioBuffer_);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        emit audioPlayError(QString("SDL_CreateAudioStream failed: %1").arg(SDL_GetError()));
        return false;
    }

    if (SDL_BindAudioStream(deviceId_, audioStream_) < 0) {
        SDL_DestroyAudioStream(audioStream_);
        SDL_CloseAudioDevice(deviceId_);
        av_freep(&audioBuffer_);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        emit audioPlayError(QString("SDL_BindAudioStream failed: %1").arg(SDL_GetError()));
        return false;
    }

    filter_ = std::make_unique<TempoFilter>();
    int threads = std::min(QThread::idealThreadCount(), 4);
    int ret = filter_->initTempoFilter(samplerate, threads, { 1, samplerate },
                                       MediaContext::DEST_CHANNEL_LAYOUT,
                                       MediaContext::DEST_SAMPLE_FMT);
    if (ret < 0 || !filter_->isInitialized()) {
        SDL_DestroyAudioStream(audioStream_);
        SDL_CloseAudioDevice(deviceId_);
        av_freep(&audioBuffer_);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        emit audioPlayError(QString("Failed to initialize TempoFilter: %1").arg(ret));
        return false;
    }

    float currentSpeed;
    {
        QMutexLocker locker(&mtx_);
        currentSpeed = speed_;
    }

    if (std::fabs(currentSpeed - 1.0f) > 0.01f) {
        ret = filter_->setTempo(currentSpeed);
        if (ret < 0) {
            SDL_DestroyAudioStream(audioStream_);
            SDL_CloseAudioDevice(deviceId_);
            av_freep(&audioBuffer_);
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            emit audioPlayError(QString("Failed to apply speed setting: %1").arg(currentSpeed));
            return false;
        }
    }

    return true;
}

void AudioPlayThread::start() {
    if (!init()) return;
    QThread::start();
}

void AudioPlayThread::stop() {
    running_.store(false);
    flush_.store(false);

    {
        QMutexLocker locker(&pauseMtx_);
        paused_ = false;
        pauseCond_.wakeAll();
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

void AudioPlayThread::pause() {
    QMutexLocker locker(&pauseMtx_);
    paused_ = true;
    SDL_PauseAudioDevice(deviceId_);
    if (audioStream_) {
        SDL_FlushAudioStream(audioStream_);
    }
}

void AudioPlayThread::resume() {
    QMutexLocker locker(&pauseMtx_);
    paused_ = false;
    pauseCond_.wakeAll();
    SDL_ResumeAudioDevice(deviceId_);
}

void AudioPlayThread::setSpeed(float speed) {
    QMutexLocker locker(&mtx_);
    if (std::fabs(speed_ - speed) < 0.01f) return;

    speed_ = speed;
    if (filter_ && filter_->isInitialized()) {
        int ret = filter_->setTempo(speed_);
        if (ret < 0) {
            emit audioPlayError(QString("Failed to set tempo: %1").arg(speed));
        }
    }
}

void AudioPlayThread::setVolume(int volume) {
    volume = qBound(0, volume, 100);
    QMutexLocker locker(&mtx_);
    volume_ = volume / 100.0f;

    if (volume_ < 0.001f && audioStream_) {
        SDL_ClearAudioStream(audioStream_);
    }
}

void AudioPlayThread::onFlushStream() {
    flush_.store(true);
}

void AudioPlayThread::run() {
    running_.store(true);
    SDL_ResumeAudioDevice(deviceId_);

    while (running_.load()) {
        bool shouldPause;
        {
            QMutexLocker locker(&pauseMtx_);
            shouldPause = paused_;
        }

        if (shouldPause) {
            QMutexLocker lock(&pauseMtx_);
            if (running_.load()) {
                pauseCond_.wait(&pauseMtx_, 100);
            }
            continue;
        }

        bool shouldFlush = flush_.exchange(false);
        if (shouldFlush) {
            SDL_ClearAudioStream(audioStream_);
            continue;
        }

        int processedSize = processFrame(audioBuffer_, bufferSize_);
        if (processedSize <= 0) {
            continue;
        }

        processVolume(audioBuffer_, processedSize);

        int result = SDL_PutAudioStreamData(audioStream_, audioBuffer_, processedSize);
        if (result < 0) {
            continue;
        }

        int remainBuffer = SDL_GetAudioStreamQueued(audioStream_);
        if (remainBuffer > 0) {
            float currentSpeed;
            {
                QMutexLocker locker(&mtx_);
                currentSpeed = speed_;
            }

            int sleepTime = calculateSleepTime(remainBuffer, processedSize, currentSpeed);
            if (sleepTime > 0) {
                msleep(sleepTime);
                LOG_INFO(sleepTime);
            }
        }
    }

    SDL_PauseAudioDevice(deviceId_);
}

int AudioPlayThread::calculateSleepTime(int remainBuffer, int processedSize, float currentSpeed) {
    if (remainBuffer <= 0 || processedSize <= 0 || currentSpeed <= 0.01f) {
        return 0;
    }

    double bufferPlayTimeMs = remainBuffer * usPerBytes_ / 1000.0;
    double processedPlayTimeMs = processedSize * usPerBytes_ / 1000.0;
    double targetBufferMs = audioBaseDuration_ * 2.5;

    if (currentSpeed > 1.0f) {
        targetBufferMs = audioBaseDuration_ * (1.5 + 0.5 / currentSpeed);
    }
    else if (currentSpeed < 1.0f) {
        targetBufferMs = audioBaseDuration_ * (2.0 + 1.0 * (1.0f - currentSpeed));
    }

    double bufferDiff = bufferPlayTimeMs - targetBufferMs;

    double baseSleepMs = processedPlayTimeMs;
    double adjustmentFactor = 1.0;

    if (bufferDiff > targetBufferMs * 0.5) {
        adjustmentFactor = 1.5;
    }
    else if (bufferDiff > targetBufferMs * 0.2) {
        adjustmentFactor = 1.2;
    }
    else if (bufferDiff < -targetBufferMs * 0.5) {
        adjustmentFactor = 0.3;
    }
    else if (bufferDiff < -targetBufferMs * 0.2) {
        adjustmentFactor = 0.7;
    }
    double finalSleepMs = baseSleepMs * adjustmentFactor - audioBaseDelay_;

    int maxSleepMs = audioBaseDuration_ * 3;
    int sleepTimeMs = static_cast<int>(std::max(0.0, std::min(finalSleepMs, static_cast<double>(maxSleepMs))));

    return sleepTimeMs;
}

int AudioPlayThread::processFrame(uint8_t* buffer, int bufferSize) {
    if (!buffer || !filter_) {
        return 0;
    }

    AVFrame* frame = buffer_->dequeue<AVFrame>(MediaType::AudioFrame);
    if (!frame) {
        return 0;
    }

    int ret = filter_->addFrame(frame);
    if (ret < 0) {
        av_frame_free(&frame);
        return 0;
    }


    AVFrame* outputFrame = av_frame_alloc();
    if (!outputFrame) {
        av_frame_free(&frame);
        return 0;
    }

    outputFrame->pts = frame->pts;
    outputFrame->duration = frame->duration;

    av_frame_free(&frame);

    ret = filter_->getFrame(outputFrame);
    if (ret < 0) {
        av_frame_free(&outputFrame);
        if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            emit audioPlayError(QString("Filter getFrame failed: %1").arg(ret));
        }
        return 0;
    }

    int dataSize = av_samples_get_buffer_size(nullptr, outputFrame->ch_layout.nb_channels,
                                              outputFrame->nb_samples,
                                              static_cast<AVSampleFormat>(outputFrame->format), 1);

    if (dataSize <= 0 || dataSize > bufferSize) {
        av_frame_free(&outputFrame);
        return 0;
    }

    memcpy(buffer, outputFrame->data[0], dataSize);

    if (outputFrame->pts != AV_NOPTS_VALUE) {
        double pts = av_q2d(outputFrame->time_base) * outputFrame->pts;
        double duration = av_q2d(outputFrame->time_base) * outputFrame->duration;
        emit updateAudioClock(pts, duration);
    }

    av_frame_free(&outputFrame);
    return dataSize;
}

int AudioPlayThread::processVolume(uint8_t* buffer, int size) {
    if (!buffer || size <= 0) {
        return 0;
    }

    float vol;
    {
        QMutexLocker locker(&mtx_);
        vol = volume_;
    }

    if (vol < 0.001f) {
        memset(buffer, 0, size);
        return size;
    }

    if (std::fabs(vol - 1.0f) < 0.001f) {
        return size;
    }

    int16_t* samples = reinterpret_cast<int16_t*>(buffer);
    int sampleCount = size / sizeof(int16_t);

    const int BLOCK_SIZE = 256;
    for (int i = 0; i < sampleCount; i += BLOCK_SIZE) {
        int blockEnd = std::min(i + BLOCK_SIZE, sampleCount);
        for (int j = i; j < blockEnd; j++) {
            int32_t temp = static_cast<int32_t>(samples[j] * vol);
            temp = std::min(std::max(temp, static_cast<int32_t>(INT16_MIN)), static_cast<int32_t>(INT16_MAX));
            samples[j] = static_cast<int16_t>(temp);
        }
    }

    return size;
}

void AudioPlayThread::cleanup() {
    if (audioStream_) {
        SDL_DestroyAudioStream(audioStream_);
        audioStream_ = nullptr;
    }

    if (deviceId_) {
        SDL_CloseAudioDevice(deviceId_);
        deviceId_ = 0;
    }

    if (audioBuffer_) {
        av_freep(&audioBuffer_);
        audioBuffer_ = nullptr;
    }

    filter_.reset();

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}