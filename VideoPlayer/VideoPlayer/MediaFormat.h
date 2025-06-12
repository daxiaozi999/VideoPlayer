#pragma once

#include <QString>
#include <QStringList>
#include <QThread>
#include <memory>
#include <algorithm>
#include "SDK.h"

namespace media {

    struct VideoParameters {
        int width = 1920;
        int height = 1080;
        int streamIndex = -1;
        int64_t bitrate = 1280000;
        AVRational framerate = { 30, 1 };
        AVRational timebase = { 1, 30000 };
        AVPixelFormat pixfmt = AV_PIX_FMT_YUV420P;
    };

    struct AudioParameters {
        int samplerate = 48000;
        int framesize = 1024;
        int streamIndex = -1;
        int64_t bitrate = 128000;
        AVRational timebase = { 1, 48000 };
        AVChannelLayout chlayout = AV_CHANNEL_LAYOUT_STEREO;
        AVSampleFormat samplefmt = AV_SAMPLE_FMT_S16;

        ~AudioParameters() {
            av_channel_layout_uninit(&chlayout);
        }
    };

    enum class StreamType {
        NONE,
        VOD,
        LIVE
    };

    class MediaFormat {
    public:
        MediaFormat(const MediaFormat&) = delete;
        MediaFormat& operator=(const MediaFormat&) = delete;
        MediaFormat(MediaFormat&&) = delete;
        MediaFormat& operator=(MediaFormat&&) = delete;

        MediaFormat();
        ~MediaFormat();
        void cleanup();

        int openLocalFile(const QString& filePath);
        int openNetworkStream(const QString& url, int threadCount = 0);

        bool hasVideoStream() const { return videoParams_.streamIndex != -1; }
        bool hasAudioStream() const { return audioParams_.streamIndex != -1; }

        int64_t getTotalTime()                      const { return totalTime_; }
        StreamType getStreamType()                  const { return streamType_; }
        const VideoParameters& getVideoParameters() const { return videoParams_; }
        const AudioParameters& getAudioParameters() const { return audioParams_; }
        AVFormatContext* getFormatContext()         const { return fmtCtx_.get(); }

    private:
        void extractParameters();
        QString extractProtocol(const QString& url) const;
        bool isLiveStream(const QString& url, AVFormatContext* fmtCtx) const;
        int getOptimalThreadCount(int requestedThreads) const;
        AVDictionary* createNetworkOptions(const QString& protocol, int threadCount) const;

    private:
        QString filePath_;
        QString streamUrl_;
        int64_t totalTime_;
        StreamType streamType_;
        VideoParameters videoParams_;
        AudioParameters audioParams_;
        std::shared_ptr<AVFormatContext> fmtCtx_;
    };

} // namespace media