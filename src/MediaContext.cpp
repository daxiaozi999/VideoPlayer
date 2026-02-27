#include "MediaContext.h"

std::unique_ptr<MediaContext> MediaContext::instance_;
std::once_flag MediaContext::flag_;

MediaContext::MediaContext()
    : url_("")
    , error_("")
    , mediaInput_(std::make_unique<media::MediaInput>())
    , mediaDecoder_(std::make_unique<media::MediaDecoder>())
    , mediaResampler_(std::make_unique<media::MediaResampler>()) {
}

MediaContext::~MediaContext() {
    reset();
}

MediaContext* MediaContext::instance() {
    std::call_once(flag_, []() {
        instance_.reset(new MediaContext());
        });
    return instance_.get();
}

int MediaContext::playFileStream(const std::string& url) {
    std::lock_guard<std::mutex> locker(mutex_);
    if (url.empty()) {
        error_ = "File url is NULL";
        return AVERROR(EINVAL);
    }

    url_.clear();
    url_ = url;

    int ret = mediaInput_->openFileStream(url);
    if (ret < 0) {
        error_ = "Open file failed: " + url;
        return ret;
    }

    if (!mediaInput_->hasVideoStream() && !mediaInput_->hasAudioStream()) {
        error_ = "Not found video and audio: " + url;
        return AVERROR(EINVAL);
    }

    ret = openDecoder();
    if (ret < 0) {
        return ret;
    }

    ret = openResampler();
    if (ret < 0) {
        return ret;
    }

    return 0;
}

int MediaContext::playNetworkStream(const std::string& url) {
    std::lock_guard<std::mutex> locker(mutex_);
    if (url.empty()) {
        error_ = "Network url is NULL";
        return AVERROR(EINVAL);
    }

    url_.clear();
    url_ = url;

    int ret = mediaInput_->openNetworkStream(url);
    if (ret < 0) {
        error_ = "Open network failed: " + url;
        return ret;
    }

    if (!mediaInput_->hasVideoStream() && !mediaInput_->hasAudioStream()) {
        error_ = "Not found video and audio: " + url;
        return AVERROR(EINVAL);
    }

    ret = openDecoder();
    if (ret < 0) {
        return ret;
    }

    ret = openResampler();
    if (ret < 0) {
        return ret;
    }

    return 0;
}

void MediaContext::reset() {
    std::lock_guard<std::mutex> locker(mutex_);

    url_.clear();
    error_.clear();

    mediaResampler_.reset();
    mediaDecoder_.reset();
    mediaInput_.reset();

    mediaInput_ = std::make_unique<media::MediaInput>();
    mediaDecoder_ = std::make_unique<media::MediaDecoder>();
    mediaResampler_ = std::make_unique<media::MediaResampler>();
}

std::string MediaContext::url() const {
    std::lock_guard<std::mutex> locker(mutex_);
    return url_;
}

std::string MediaContext::error() const {
    std::lock_guard<std::mutex> locker(mutex_);
    return error_;
}

int MediaContext::openDecoder() {
    if (mediaInput_->hasVideoStream()) {
        int ret = mediaDecoder_->openVideoDecoder(mediaInput_->inputContext());
        if (ret < 0) {
            error_ = "Open video decoder failed";
            return ret;
        }
    }

    if (mediaInput_->hasAudioStream()) {
        int ret = mediaDecoder_->openAudioDecoder(mediaInput_->inputContext());
        if (ret < 0) {
            error_ = "Open audio decoder failed";
            return ret;
        }
    }

    return 0;
}

int MediaContext::openResampler() {
    if (mediaDecoder_->videoDecoder()) {
        const media::VideoParams& vp = mediaInput_->videoParams();
        int ret = mediaResampler_->configSwsContext(vp.width, vp.height, vp.pixfmt,
                                                    vp.width, vp.height, TARGET_PIXEL_FORMAT);
        if (ret < 0) {
            error_ = "Config sws context failed";
            return ret;
        }
    }

    if (mediaDecoder_->audioDecoder()) {
        const media::AudioParams& ap = mediaInput_->audioParams();
        int ret = mediaResampler_->configSwrContext(ap.samplerate, ap.chlayout, ap.samplefmt,
                                                    ap.samplerate, TARGET_CHANNEL_LAYOUT, TARGET_SAMPLE_FORMAT);
        if (ret < 0) {
            error_ = "Config swr context failed";
            return ret;
        }
    }

    return 0;
}

media::MediaInput* MediaContext::mediaInput() const {
    std::lock_guard<std::mutex> locker(mutex_);
    return mediaInput_.get();
}

media::MediaDecoder* MediaContext::mediaDecoder() const {
    std::lock_guard<std::mutex> locker(mutex_);
    return mediaDecoder_.get();
}

media::MediaResampler* MediaContext::mediaResampler() const {
    std::lock_guard<std::mutex> locker(mutex_);
    return mediaResampler_.get();
}