#pragma once

#include <mutex>
#include <string>
#include <memory>
#include "FFmpeg.h"
#include "MediaInput.h"
#include "MediaDecoder.h"
#include "MediaResampler.h"

#define MCTX() MediaContext::instance()

class MediaContext {
public:
    MediaContext(const MediaContext&) = delete;
    MediaContext& operator=(const MediaContext&) = delete;
    MediaContext(MediaContext&&) = delete;
    MediaContext& operator=(MediaContext&&) = delete;

    static constexpr AVPixelFormat TARGET_PIXEL_FORMAT = AV_PIX_FMT_YUV420P;
    static constexpr AVSampleFormat TARGET_SAMPLE_FORMAT = AV_SAMPLE_FMT_S16;
    static constexpr AVChannelLayout TARGET_CHANNEL_LAYOUT = AV_CHANNEL_LAYOUT_STEREO;

    ~MediaContext();
    static MediaContext* instance();

    int playFileStream(const std::string& url);
    int playNetworkStream(const std::string& url);
    void reset();

    std::string url() const;
    std::string error() const;
    media::MediaInput* mediaInput() const;
    media::MediaDecoder* mediaDecoder() const;
    media::MediaResampler* mediaResampler() const;

private:
    MediaContext();

    int openDecoder();
    int openResampler();

private:
    static std::unique_ptr<MediaContext> instance_;
    static std::once_flag flag_;

    mutable std::mutex mutex_;
    std::string url_;
    std::string error_;

    std::unique_ptr<media::MediaInput> mediaInput_;
    std::unique_ptr<media::MediaDecoder> mediaDecoder_;
    std::unique_ptr<media::MediaResampler> mediaResampler_;
};