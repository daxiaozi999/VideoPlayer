#include "MediaDecoder.h"

namespace media {

    MediaDecoder::MediaDecoder()
        : hwAccelEnabled_(true)
        , vDec_(nullptr)
        , aDec_(nullptr)
        , vDecCtx_(nullptr)
        , aDecCtx_(nullptr) {
    }

    MediaDecoder::~MediaDecoder() {
        cleanup();
    }

    int MediaDecoder::openDecoder(AVFormatContext* fmtCtx, int streamIndex, bool hwAccelEnable) {
        return openDecoder(fmtCtx, streamIndex, hwAccelEnable, 0);
    }

    int MediaDecoder::openDecoder(AVFormatContext* fmtCtx, int streamIndex, bool hwAccelEnable, unsigned int threads) {
        if (!fmtCtx)  return AVERROR(EINVAL);
        if (streamIndex < 0 || static_cast<unsigned int>(streamIndex) >= fmtCtx->nb_streams) return AVERROR(EINVAL);
        if (!fmtCtx->streams[streamIndex]) return AVERROR(EINVAL);
        if (!fmtCtx->streams[streamIndex]->codecpar) return AVERROR(EINVAL);

        hwAccelEnabled_ = hwAccelEnable;
        const AVMediaType codecType = fmtCtx->streams[streamIndex]->codecpar->codec_type;

        switch (codecType) {
        case AVMEDIA_TYPE_VIDEO:
            if (vDecCtx_) {
                resetVideoDecoder();
            }
            return openInternalDecoder(fmtCtx, streamIndex, vDec_, vDecCtx_, threads);
        case AVMEDIA_TYPE_AUDIO:
            if (aDecCtx_) {
                resetAudioDecoder();
            }
            return openInternalDecoder(fmtCtx, streamIndex, aDec_, aDecCtx_, threads);
        default:
            return AVERROR(EINVAL);
        }
    }

    void MediaDecoder::flushAllDecoder() {
        if (vDecCtx_) {
            avcodec_flush_buffers(vDecCtx_.get());
        }
        if (aDecCtx_) {
            avcodec_flush_buffers(aDecCtx_.get());
        }
    }

    void MediaDecoder::resetVideoDecoder() {
        vDec_ = nullptr;
        vDecCtx_.reset();
    }

    void MediaDecoder::resetAudioDecoder() {
        aDec_ = nullptr;
        aDecCtx_.reset();
    }

    void MediaDecoder::cleanup() {
        resetVideoDecoder();
        resetAudioDecoder();
    }

    int MediaDecoder::openInternalDecoder(AVFormatContext* fmtCtx, int streamIndex,
                                          const AVCodec*& dec, std::shared_ptr<AVCodecContext>& decCtx,
                                          unsigned int threads) {
        if (fmtCtx->streams[streamIndex]->codecpar->codec_id == AV_CODEC_ID_NONE) return AVERROR_DECODER_NOT_FOUND;

        dec = avcodec_find_decoder(fmtCtx->streams[streamIndex]->codecpar->codec_id);
        if (!dec) {
            return AVERROR_DECODER_NOT_FOUND;
        }

        AVCodecContext* ctx = avcodec_alloc_context3(dec);
        if (!ctx) {
            return AVERROR(ENOMEM);
        }

        int ret = avcodec_parameters_to_context(ctx, fmtCtx->streams[streamIndex]->codecpar);
        if (ret < 0) {
            avcodec_free_context(&ctx);
            return ret;
        }

        ctx->time_base = fmtCtx->streams[streamIndex]->time_base;
        ctx->thread_count = (threads == 0) ? 0 : std::max(1u, threads);

        const AVMediaType codecType = fmtCtx->streams[streamIndex]->codecpar->codec_type;
        if (codecType == AVMEDIA_TYPE_VIDEO) {
            ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        }
        else {
            ctx->thread_type = FF_THREAD_FRAME;
        }

        AVDictionary* opts = nullptr;
        if (hwAccelEnabled_ && codecType == AVMEDIA_TYPE_VIDEO) {
            const char* hwaccel = nullptr;
#if defined(__APPLE__)
            hwaccel = "videotoolbox";
#elif defined(_WIN32)
            hwaccel = "dxva2";  // hwaccel = "d3d11va";
#elif defined(__linux__)
            hwaccel = "vaapi";  // hwaccel = "vdpau";
#endif
            if (hwaccel) {
                av_dict_set(&opts, "hwaccel", hwaccel, 0);
            }
        }

        ret = avcodec_open2(ctx, dec, &opts);
        av_dict_free(&opts);

        if (ret < 0) {
            avcodec_free_context(&ctx);
            return ret;
        }

        decCtx = std::shared_ptr<AVCodecContext>(ctx,
            [](AVCodecContext* p) {
                if (p) {
                    avcodec_free_context(&p);
                }
            });

        return 0;
    }

} // namespace media