#include "MediaContext.h"

std::unique_ptr<MediaContext> MediaContext::instance_ = nullptr;
std::once_flag MediaContext::flag_;

MediaContext::MediaContext()
    : filePath_("")
    , streamUrl_("")
    , lastError_("")
    , format_(std::make_unique<media::MediaFormat>())
    , decoder_(std::make_unique<media::MediaDecoder>())
    , resampler_(std::make_unique<media::MediaResampler>()) {
}

MediaContext::~MediaContext() {
    cleanup();
}

MediaContext& MediaContext::getInstance() {
    std::call_once(flag_, []() {
		instance_.reset(new MediaContext());
        });
    return *instance_;
}

int MediaContext::playLocalFile(const QString& filePath) {
    if (filePath.isEmpty()) {
        QMutexLocker locker(&mtx_);
        lastError_ = "File path is empty";
        return AVERROR(EINVAL);
    }

    cleanup();

    {
        QMutexLocker locker(&mtx_);
        filePath_ = filePath;
    }

    int ret = format_->openLocalFile(filePath);
    if (ret < 0) {
        QMutexLocker locker(&mtx_);
        lastError_ = QString("Failed to open file: %1").arg(filePath);
        return ret;
    }

    if (!format_->hasVideoStream() && !format_->hasAudioStream()) {
        QMutexLocker locker(&mtx_);
        lastError_ = "No found video and audio stream";
        return AVERROR(EINVAL);
    }

    return openComponents();
}

int MediaContext::playNetworkStream(const QString& url) {
    if (url.isEmpty()) {
        QMutexLocker locker(&mtx_);
        lastError_ = "URL is empty";
        return AVERROR(EINVAL);
    }

    cleanup();

    {
        QMutexLocker locker(&mtx_);
        streamUrl_ = url;
    }

    int ret = format_->openNetworkStream(url, 4);
    if (ret < 0) {
        QMutexLocker locker(&mtx_);
        lastError_ = QString("Failed to open stream: %1").arg(url);
        return ret;
    }

    if (!format_->hasVideoStream() && !format_->hasAudioStream()) {
        QMutexLocker locker(&mtx_);
        lastError_ = "No found video and audio stream";
        return AVERROR(EINVAL);
    }

    return openComponents();
}

void MediaContext::cleanup() {
    QMutexLocker locker(&mtx_);
    filePath_.clear();
    streamUrl_.clear();
    lastError_.clear();

    locker.unlock();

    if (format_) format_->cleanup();
    if (decoder_) decoder_->cleanup();
    if (resampler_) resampler_->cleanup();
}

int MediaContext::openDecoder() {
    if (!format_->getFormatContext()) {
        QMutexLocker locker(&mtx_);
        lastError_ = "Format context not available";
        return AVERROR(EINVAL);
    }

    int ret = 0;

    if (format_->hasVideoStream()) {
        const media::VideoParameters vParams = format_->getVideoParameters();
        ret = decoder_->openDecoder(format_->getFormatContext(), vParams.streamIndex, true);
        if (ret < 0) {
            QMutexLocker locker(&mtx_);
            lastError_ = "Failed to open video decoder";
            return ret;
        }
    }

    if (format_->hasAudioStream()) {
        const media::AudioParameters aParams = format_->getAudioParameters();
        ret = decoder_->openDecoder(format_->getFormatContext(), aParams.streamIndex, true);
        if (ret < 0) {
            QMutexLocker locker(&mtx_);
            lastError_ = "Failed to open audio decoder";
            return ret;
        }
    }

    return 0;
}

int MediaContext::openResampler() {
    int ret = 0;

    if (decoder_->hasVideoDecoder()) {
        const media::VideoParameters vParams = format_->getVideoParameters();
        ret = resampler_->autoConfigSwsContext(vParams.width, vParams.height, vParams.pixfmt,
                                               vParams.width, vParams.height, DEST_PIX_FMT);
        if (ret < 0) {
            QMutexLocker locker(&mtx_);
            lastError_ = "Failed to configure video resampler";
            return ret;
        }
    }

    if (decoder_->hasAudioDecoder()) {
        const media::AudioParameters aParams = format_->getAudioParameters();
        ret = resampler_->configSwrContext(DEST_CHANNEL_LAYOUT, DEST_SAMPLE_FMT, aParams.samplerate,
                                           aParams.chlayout, aParams.samplefmt, aParams.samplerate);
        if (ret < 0) {
            QMutexLocker locker(&mtx_);
            lastError_ = "Failed to configure audio resampler";
            return ret;
        }
    }

    return 0;
}

int MediaContext::openComponents() {
    int ret = openDecoder();
    if (ret < 0) {
        return ret;
    }

    ret = openResampler();
    if (ret < 0) {
        return ret;
    }

    return 0;
}

QString MediaContext::getFilePath() const {
    QMutexLocker locker(&mtx_);
    return filePath_;
}

QString MediaContext::getStreamUrl() const {
    QMutexLocker locker(&mtx_);
    return streamUrl_;
}

QString MediaContext::getLastError() const {
    QMutexLocker locker(&mtx_);
    return lastError_;
}

bool MediaContext::hasVideo() const {
    QMutexLocker locker(&mtx_);
    return format_->hasVideoStream();
}

bool MediaContext::hasAudio() const {
    QMutexLocker locker(&mtx_);
    return format_->hasAudioStream();
}

int64_t MediaContext::getTotalTime() const {
    QMutexLocker locker(&mtx_);
    return format_->getTotalTime();
}

media::StreamType MediaContext::getStreamType() const {
    QMutexLocker locker(&mtx_);
    return format_->getStreamType();
}

const media::VideoParameters& MediaContext::getVideoParameters() const {
    QMutexLocker locker(&mtx_);
    return format_->getVideoParameters();
}

const media::AudioParameters& MediaContext::getAudioParameters() const {
    QMutexLocker locker(&mtx_);
    return format_->getAudioParameters();
}

SwsContext* MediaContext::getSwsContext() const {
    return resampler_->getSwsContext();
}

SwrContext* MediaContext::getSwrContext() const {
    return resampler_->getSwrContext();
}

AVCodecContext* MediaContext::getVideoDecoder() const {
    return decoder_->getVideoDecoder();
}

AVCodecContext* MediaContext::getAudioDecoder() const {
    return decoder_->getAudioDecoder();
}

AVFormatContext* MediaContext::getFormatContext() const {
    return format_->getFormatContext();
}