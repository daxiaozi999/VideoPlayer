#include "AudioPlayThread.h"

AudioPlayThread::AudioPlayThread(QObject* parent, std::shared_ptr<MediaBuffer> buffer)
    : QThread(parent)
    , buffer_(buffer)
    , filter_(nullptr)
    , SDLAudioStream_(nullptr)
    , SDLAudioBuffer_(nullptr)
    , SDLAudioBufferSize_(0)
    , initError_("")
    , speed_(1.0f)
    , volume_(0.7f)
    , inited_(false)
    , flush_(false)
    , paused_(false)
    , running_(false)
    , started_(false)
    , currentTime_(0.0) {

    do {
        if (!buffer_) {
            initError_ = "Media buffer is NULL";
            break;
        }

        if (!SDL_Init(SDL_INIT_AUDIO)) {
            initError_ = QString("SDL init audio failed: %1").arg(SDL_GetError());
            break;
        }

        int samplerate = MCTX()->mediaInput()->audioParams().samplerate;
        int channels = MediaContext::TARGET_CHANNEL_LAYOUT.nb_channels;

        if (samplerate <= 0 || channels <= 0) {
            initError_ = "Invalid samplerate or channels";
            break;
        }

        SDLAudioBufferSize_ = samplerate * 2 * channels * sizeof(int16_t);
        SDLAudioBuffer_ = static_cast<uint8_t*>(av_malloc(SDLAudioBufferSize_));
        if (!SDLAudioBuffer_) {
            initError_ = "Malloc SDL audio buffer failed";
            break;
        }

        SDL_AudioSpec spec;
        SDL_zero(spec);
        spec.freq = samplerate;
        spec.format = SDL_AUDIO_S16;
        spec.channels = static_cast<uint8_t>(channels);

        SDLAudioStream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                                    &spec,
                                                    &AudioPlayThread::audioStreamCallback,
                                                    this);
        if (!SDLAudioStream_) {
            initError_ = QString("Open SDL audio device stream failed: %1").arg(SDL_GetError());
            break;
        }

        filter_ = std::make_unique<media::TempoFilter>();
        int ret = filter_->init(samplerate,
                                { 1, samplerate },
                                MediaContext::TARGET_CHANNEL_LAYOUT,
                                MediaContext::TARGET_SAMPLE_FORMAT,
                                qBound(1, QThread::idealThreadCount(), 4));
        if (ret < 0 || !filter_->isInited()) {
            initError_ = "Tempo filter init failed";
            break;
        }

        ret = filter_->setTempo(speed_);
        if (ret < 0) {
            initError_ = "Tempo filter set tempo failed";
            break;
        }

        inited_.store(true);

    } while (false);

    if (!inited_.load()) {
        cleanup();
    }
}

AudioPlayThread::~AudioPlayThread() {
    stop();
    cleanup();
}

void AudioPlayThread::start() {
    if (started_.exchange(true)) {
        return;
    }

    if (!inited_.load()) {
        emit audioPlayError(initError_);
        return;
    }

    QThread::start();
}

void AudioPlayThread::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    flush_.store(false);
    requestInterruption();
    paused_.store(false);

    if (!wait(3000)) {
        terminate();
        wait(1000);
    }

    started_.store(false);
}

void AudioPlayThread::pause() {
    paused_.store(true);

    if (SDLAudioStream_) {
        SDL_PauseAudioStreamDevice(SDLAudioStream_);
    }
}

void AudioPlayThread::resume() {
    paused_.store(false);

    if (SDLAudioStream_) {
        SDL_ResumeAudioStreamDevice(SDLAudioStream_);
    }
}

void AudioPlayThread::setSpeed(float speed) {
    {
        QMutexLocker locker(&mutex_);
        if (qAbs(speed_ - speed) < 0.01f) {
            return;
        }
        speed_ = speed;
    }

    if (filter_ && filter_->isInited()) {
        int ret = filter_->setTempo(speed);
        if (ret < 0) {
            emit audioPlayError("Tempo filter set tempo failed");
        }
    }
}

void AudioPlayThread::setVolume(int volume) {
    volume = qBound(0, volume, 100);

    {
        QMutexLocker locker(&mutex_);
        volume_ = volume / 100.0f;
    }

    if (volume_ < 0.001f && SDLAudioStream_) {
        SDL_ClearAudioStream(SDLAudioStream_);
    }
}

double AudioPlayThread::getCurrentTime() const {
    return currentTime_.load();
}

void AudioPlayThread::onFlushStream() {
    flush_.store(true);

    if (SDLAudioStream_) {
        SDL_ClearAudioStream(SDLAudioStream_);
    }
}

void AudioPlayThread::run() {
    running_.store(true);

    if (SDLAudioStream_) {
        SDL_ResumeAudioStreamDevice(SDLAudioStream_);
    }

    while (running_.load() && !isInterruptionRequested()) {
        msleep(50);
    }

    if (SDLAudioStream_) {
        SDL_PauseAudioStreamDevice(SDLAudioStream_);
    }

    running_.store(false);
}

int AudioPlayThread::processFrame(uint8_t* buffer, int size) {
    if (!buffer || !filter_ || !buffer_ || size <= 0) {
        return 0;
    }

    AVFrame* srcFrame = reinterpret_cast<AVFrame*>(buffer_->dequeue<media::AUDIO, media::DECODING>());
    if (!srcFrame) {
        return 0;
    }

    AVFrame* dstFrame = av_frame_alloc();
    if (!dstFrame) {
        av_frame_free(&srcFrame);
        return 0;
    }

    double pts = srcFrame->pts * av_q2d(srcFrame->time_base);
    double duration = srcFrame->duration * av_q2d(srcFrame->time_base);

    int dstSize = 0;
    do {
        int ret = filter_->addFrame(srcFrame);
        if (ret < 0) {
            break;
        }

        ret = filter_->getFrame(dstFrame);
        if (ret < 0) {
            break;
        }

        dstSize = av_samples_get_buffer_size(nullptr,
                                             dstFrame->ch_layout.nb_channels,
                                             dstFrame->nb_samples,
                                             static_cast<AVSampleFormat>(dstFrame->format),
                                             1);
        if (dstSize <= 0 || dstSize > size) {
            break;
        }

        memcpy(buffer, dstFrame->data[0], dstSize);
        processVolume(buffer, dstSize);

    } while (false);

    av_frame_free(&srcFrame);
    av_frame_free(&dstFrame);

    currentTime_.store(pts);
    emit updateAudioClock(pts, duration);

    return dstSize;
}

int AudioPlayThread::processVolume(uint8_t* buffer, int size) {
    if (!buffer || size <= 0) {
        return 0;
    }

    float volume;
    {
        QMutexLocker locker(&mutex_);
        volume = volume_;
    }

    if (volume < 0.001f) {
        memset(buffer, 0, size);
        return size;
    }

    if (qAbs(volume - 1.0f) < 0.001f) {
        return size;
    }

    int16_t* samples = reinterpret_cast<int16_t*>(buffer);
    int total = size / sizeof(int16_t);

    for (int i = 0; i < total; i += 256) {
        int blockEnd = qMin(i + 256, total);
        for (int j = i; j < blockEnd; ++j) {
            int32_t temp = static_cast<int32_t>(samples[j] * volume);
            temp = qBound(static_cast<int32_t>(INT16_MIN), temp, static_cast<int32_t>(INT16_MAX));
            samples[j] = static_cast<int16_t>(temp);
        }
    }

    return size;
}

void SDLCALL AudioPlayThread::audioStreamCallback(void* userdata, SDL_AudioStream* stream, int additional, int total) {
    if (!userdata) {
        return;
    }

    AudioPlayThread* pthis = static_cast<AudioPlayThread*>(userdata);

    if (!pthis->running_.load() || pthis->paused_.load()) {
        SDL_Delay(1);
        return;
    }

    if (pthis->flush_.exchange(false)) {
        SDL_ClearAudioStream(stream);
        return;
    }

    if (additional <= 0) {
        SDL_Delay(1);
        return;
    }

    int size = pthis->processFrame(pthis->SDLAudioBuffer_, pthis->SDLAudioBufferSize_);
    if (size <= 0) {
        return;
    }

    SDL_PutAudioStreamData(stream, pthis->SDLAudioBuffer_, size);
}

void AudioPlayThread::cleanup() {
    if (SDLAudioStream_) {
        SDL_DestroyAudioStream(SDLAudioStream_);
        SDLAudioStream_ = nullptr;
    }

    if (SDLAudioBuffer_) {
        av_freep(&SDLAudioBuffer_);
        SDLAudioBuffer_ = nullptr;
    }

    if (SDL_WasInit(SDL_INIT_AUDIO)) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}