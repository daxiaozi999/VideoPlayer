#pragma once

#include <QUrl>
#include <QString>
#include <QMutex>
#include <iostream>
#include <memory>
#include <mutex>
#include "SDK.h"
#include "MediaFormat.h"
#include "MediaDecoder.h"
#include "MediaResampler.h"

class MediaContext {
public:
	MediaContext(const MediaContext&) = delete;
	MediaContext& operator=(const MediaContext&) = delete;
	MediaContext(MediaContext&&) = delete;
	MediaContext& operator=(MediaContext&&) = delete;

    static constexpr AVPixelFormat DEST_PIX_FMT          = AV_PIX_FMT_YUV420P;
    static constexpr AVSampleFormat DEST_SAMPLE_FMT      = AV_SAMPLE_FMT_S16;
    static constexpr AVChannelLayout DEST_CHANNEL_LAYOUT = AV_CHANNEL_LAYOUT_STEREO;
    
    ~MediaContext();
    void cleanup();
    static MediaContext& getInstance();

	int playLocalFile(const QString& filePath);
	int playNetworkStream(const QString& streamUrl);

    QString getFilePath() const;
	QString getStreamUrl() const;
    QString getLastError() const;

    bool hasVideo() const;
	bool hasAudio() const;

    int64_t getTotalTime() const;
	media::StreamType getStreamType() const;
    const media::VideoParameters& getVideoParameters() const;
    const media::AudioParameters& getAudioParameters() const;

    SwsContext* getSwsContext() const;
    SwrContext* getSwrContext() const;
    AVCodecContext* getVideoDecoder() const;
    AVCodecContext* getAudioDecoder() const;
    AVFormatContext* getFormatContext() const;

private:
    MediaContext();
    int openDecoder();
    int openResampler();
    int openComponents();

private:
    static std::unique_ptr<MediaContext> instance_;
    static std::once_flag flag_;
    mutable QMutex mtx_;
    QString filePath_;
    QString streamUrl_;
    QString lastError_;
    std::unique_ptr<media::MediaFormat> format_;
    std::unique_ptr<media::MediaDecoder> decoder_;
    std::unique_ptr<media::MediaResampler> resampler_;
};
