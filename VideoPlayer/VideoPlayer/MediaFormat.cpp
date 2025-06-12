#include "MediaFormat.h"

namespace media {

    MediaFormat::MediaFormat()
        : filePath_("")
        , streamUrl_("")
        , totalTime_(0)
        , streamType_(StreamType::NONE)
        , videoParams_()
        , audioParams_()
        , fmtCtx_(nullptr) {
        avformat_network_init();
    }

    MediaFormat::~MediaFormat() {
        cleanup();
        avformat_network_deinit();
    }

    int MediaFormat::openLocalFile(const QString& filePath) {
        if (filePath.isEmpty()) return AVERROR(EINVAL);

        cleanup();

        filePath_ = filePath;
        AVFormatContext* ctx = nullptr;
        int ret = avformat_open_input(&ctx, filePath.toUtf8().constData(), nullptr, nullptr);
        if (ret < 0) {
            return ret;
        }

        ret = avformat_find_stream_info(ctx, nullptr);
        if (ret < 0) {
            avformat_close_input(&ctx);
            return ret;
        }

        fmtCtx_ = std::shared_ptr<AVFormatContext>(ctx, [](AVFormatContext* p) {
            if (p) {
                avformat_close_input(&p);
            }
            });

        streamType_ = StreamType::VOD;
        extractParameters();

        return 0;
    }

    int MediaFormat::openNetworkStream(const QString& url, int threadCount) {
        if (url.isEmpty()) return AVERROR(EINVAL);

        cleanup();

        streamUrl_ = url;
        QString protocol = extractProtocol(url);
        int optimalThreads = getOptimalThreadCount(threadCount);
        AVDictionary* options = createNetworkOptions(protocol, optimalThreads);

        AVFormatContext* ctx = nullptr;
        int ret = avformat_open_input(&ctx, url.toUtf8().constData(), nullptr, &options);
        av_dict_free(&options);

        if (ret < 0) {
            return ret;
        }

        ret = avformat_find_stream_info(ctx, nullptr);
        if (ret < 0) {
            avformat_close_input(&ctx);
            return ret;
        }

        fmtCtx_ = std::shared_ptr<AVFormatContext>(ctx, [](AVFormatContext* ptr) {
            if (ptr) {
                avformat_close_input(&ptr);
            }
            });

        bool isLive = isLiveStream(url, fmtCtx_.get());
        streamType_ = isLive ? StreamType::LIVE : StreamType::VOD;

        extractParameters();

        return 0;
    }

    void MediaFormat::cleanup() {
        filePath_.clear();
        streamUrl_.clear();
        streamType_ = StreamType::NONE;
        totalTime_ = 0;

        videoParams_ = VideoParameters();
        audioParams_ = AudioParameters();

        fmtCtx_.reset();
    }

    void MediaFormat::extractParameters() {
        if (!fmtCtx_ || !fmtCtx_->streams) return;

        videoParams_ = VideoParameters();
        audioParams_ = AudioParameters();

        for (unsigned int i = 0; i < fmtCtx_->nb_streams; i++) {
            AVStream* stream = fmtCtx_->streams[i];
            if (!stream || !stream->codecpar) {
                continue;
            }

            AVCodecParameters* codecParams = stream->codecpar;

            if (codecParams->codec_type == AVMEDIA_TYPE_VIDEO && videoParams_.streamIndex == -1) {
                videoParams_.streamIndex = stream->index;
                videoParams_.width = codecParams->width;
                videoParams_.height = codecParams->height;
                videoParams_.bitrate = codecParams->bit_rate > 0 ? codecParams->bit_rate : 1280000;
                videoParams_.timebase = stream->time_base;
                videoParams_.pixfmt = static_cast<AVPixelFormat>(codecParams->format);

                if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
                    videoParams_.framerate = stream->avg_frame_rate;
                }
                else if (stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0) {
                    videoParams_.framerate = stream->r_frame_rate;
                }
            }
            else if (codecParams->codec_type == AVMEDIA_TYPE_AUDIO && audioParams_.streamIndex == -1) {
                audioParams_.streamIndex = stream->index;
                audioParams_.samplerate = codecParams->sample_rate;
                audioParams_.framesize = codecParams->frame_size;
                audioParams_.bitrate = codecParams->bit_rate > 0 ? codecParams->bit_rate : 128000;
                audioParams_.timebase = stream->time_base;
                audioParams_.samplefmt = static_cast<AVSampleFormat>(codecParams->format);

                av_channel_layout_copy(&audioParams_.chlayout, &codecParams->ch_layout);
            }
        }

        if (fmtCtx_->duration != AV_NOPTS_VALUE) {
            totalTime_ = fmtCtx_->duration / AV_TIME_BASE;
        }
        else if (videoParams_.streamIndex != -1) {
            AVStream* videoStream = fmtCtx_->streams[videoParams_.streamIndex];
            if (videoStream && videoStream->duration != AV_NOPTS_VALUE) {
                totalTime_ = av_rescale_q(videoStream->duration, videoStream->time_base, { 1, 1 });
            }
        }
    }

    QString MediaFormat::extractProtocol(const QString& url) const {
        if (url.isEmpty()) return QString();

        int pos = url.indexOf("://");
        if (pos == -1) {
            return QString();
        }

        return url.left(pos).toLower().trimmed();
    }

    bool MediaFormat::isLiveStream(const QString& url, AVFormatContext* fmtCtx) const {
        if (!fmtCtx) return false;

        const QString lowerUrl = url.toLower();

        static const QStringList liveProtocols = {
            "rtmp://", "rtmps://", "rtsp://", "rtsps://",
            "mms://", "mmsh://", "mmst://", "srt://", "udp://"
        };

        for (const QString& protocol : liveProtocols) {
            if (lowerUrl.startsWith(protocol)) {
                return true;
            }
        }

        static const QStringList liveKeywords = {
            "/live/", "/livestream/", "/realtime/", "/broadcast/",
            "/stream/", "/streaming/", "/webcast/", "/live-",
            "_live_", "live.", ".live"
        };

        bool hasLiveKeywords = false;
        for (const QString& keyword : liveKeywords) {
            if (lowerUrl.contains(keyword)) {
                hasLiveKeywords = true;
                break;
            }
        }

        bool isLiveFormat = false;
        bool hasNoDuration = (fmtCtx->duration == AV_NOPTS_VALUE || fmtCtx->duration <= 0);

        if (fmtCtx->iformat && fmtCtx->iformat->name) {
            QString formatName = QString(fmtCtx->iformat->name).toLower();
            isLiveFormat = formatName.contains("flv") ||
                formatName.contains("rtsp") ||
                formatName.contains("rtmp") ||
                formatName.contains("rtp") ||
                formatName.contains("mpegts") ||
                formatName.contains("hls");
        }

        if (lowerUrl.contains(".m3u8")) {
            if (hasLiveKeywords) {
                return true;
            }

            if (hasNoDuration) {
                QRegExp vodPattern("/(\\d{4,})/(\\d{6,})/|/\\d{4}-\\d{2}-\\d{2}/|/replay/|/vod/|/archive/");
                if (vodPattern.indexIn(lowerUrl) != -1) {
                    return false;
                }
                return true;
            }

            if (fmtCtx->duration != AV_NOPTS_VALUE && fmtCtx->duration < 30 * AV_TIME_BASE && hasLiveKeywords) {
                return true;
            }

            return false;
        }

        bool hasLiveCharacteristics = false;

        if (fmtCtx->metadata) {
            AVDictionaryEntry* entry = nullptr;
            while ((entry = av_dict_get(fmtCtx->metadata, "", entry, AV_DICT_IGNORE_SUFFIX))) {
                QString key = QString(entry->key).toLower();
                QString value = QString(entry->value).toLower();
                if (key.contains("live") || value.contains("live") ||
                    key.contains("stream") || value.contains("realtime")) {
                    hasLiveCharacteristics = true;
                    break;
                }
            }
        }

        if (isLiveFormat) {
            if (hasNoDuration && (hasLiveKeywords || hasLiveCharacteristics)) {
                return true;
            }
            if (hasLiveKeywords) {
                return true;
            }
        }

        if (hasLiveKeywords && hasNoDuration) {
            return true;
        }

        return false;
    }

    int MediaFormat::getOptimalThreadCount(int requestedThreads) const {
        if (requestedThreads > 0) {
            return std::min(requestedThreads, 16);
        }

        int cpuCores = QThread::idealThreadCount();
        if (cpuCores <= 0) cpuCores = 4;

        int optimalThreads = std::max(2, std::min(8, static_cast<int>(cpuCores * 0.75)));
        return optimalThreads;
    }

    AVDictionary* MediaFormat::createNetworkOptions(const QString& protocol, int threadCount) const {
        AVDictionary* options = nullptr;
        const QString lowerProtocol = protocol.toLower();

        av_dict_set_int(&options, "analyzeduration", 1000000, 0);
        av_dict_set_int(&options, "probesize", 1048576, 0);
        av_dict_set_int(&options, "threads", threadCount, 0);
        av_dict_set_int(&options, "thread_queue_size", 1024, 0);

        if (lowerProtocol == "rtmp") {
            av_dict_set_int(&options, "stimeout", 15000000, 0);
            av_dict_set_int(&options, "buffer_size", 1024 * 1024, 0);
            av_dict_set_int(&options, "reconnect", 1, 0);
            av_dict_set_int(&options, "reconnect_streamed", 1, 0);
            av_dict_set_int(&options, "reconnect_delay_max", 2, 0);
        }
        else if (lowerProtocol == "rtsp") {
            av_dict_set(&options, "rtsp_transport", "tcp", 0);
            av_dict_set_int(&options, "stimeout", 20000000, 0);
            av_dict_set_int(&options, "timeout", 30000000, 0);
            av_dict_set_int(&options, "buffer_size", 2 * 1024 * 1024, 0);
            av_dict_set_int(&options, "reorder_queue_size", 500, 0);
            av_dict_set_int(&options, "max_delay", 1000000, 0);
        }
        else if (lowerProtocol == "http" || lowerProtocol == "https") {
            av_dict_set(&options, "user_agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36", 0);
            av_dict_set_int(&options, "timeout", 30000000, 0);
            av_dict_set_int(&options, "reconnect", 1, 0);
            av_dict_set_int(&options, "reconnect_streamed", 1, 0);
            av_dict_set_int(&options, "reconnect_delay_max", 3, 0);
            av_dict_set_int(&options, "buffer_size", 1024 * 1024, 0);
            av_dict_set_int(&options, "multiple_requests", 1, 0);
            av_dict_set_int(&options, "seekable", 0, 0);
        }
        else if (lowerProtocol == "udp") {
            av_dict_set_int(&options, "buffer_size", 8 * 1024 * 1024, 0);
            av_dict_set_int(&options, "fifo_size", 2000000, 0);
            av_dict_set_int(&options, "overrun_nonfatal", 1, 0);
            av_dict_set_int(&options, "timeout", 8000000, 0);
        }
        else if (lowerProtocol == "tcp") {
            av_dict_set_int(&options, "timeout", 20000000, 0);
            av_dict_set_int(&options, "buffer_size", 2 * 1024 * 1024, 0);
            av_dict_set_int(&options, "tcp_nodelay", 1, 0);
        }
        else {
            av_dict_set_int(&options, "timeout", 20000000, 0);
            av_dict_set_int(&options, "buffer_size", 1024 * 1024, 0);
        }

        return options;
    }

} // namespace media