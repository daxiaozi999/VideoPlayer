#include "MediaResampler.h"

namespace media {

    MediaResampler::MediaResampler() : swsCtx_(nullptr), swrCtx_(nullptr) {
        av_channel_layout_uninit(&swrParams_.inChLayout);
        av_channel_layout_uninit(&swrParams_.outChLayout);
    }

    MediaResampler::~MediaResampler() {
        cleanup();
    }

    int MediaResampler::configSwsContext(int srcW, int srcH, AVPixelFormat srcFmt,
                                         int dstW, int dstH, AVPixelFormat dstFmt,
                                         int flags) {
        if (srcW <= 0 || srcH <= 0 || srcFmt == AV_PIX_FMT_NONE ||
            dstW <= 0 || dstH <= 0 || dstFmt == AV_PIX_FMT_NONE) {
            return AVERROR(EINVAL);
        }

        if (swsCtx_ && !isSwsParamsChanged(srcW, srcH, srcFmt, dstW, dstH, dstFmt, flags)) {
            return 0;
        }

        resetSwsContext();

        SwsContext* ctx = sws_getContext(srcW, srcH, srcFmt,
                                         dstW, dstH, dstFmt,
                                         flags, nullptr, nullptr, nullptr);
        if (!ctx) {
            return AVERROR(ENOMEM);
        }

        swsParams_.srcW   = srcW;
        swsParams_.srcH   = srcH;
        swsParams_.srcFmt = srcFmt;
        swsParams_.dstW   = dstW;
        swsParams_.dstH   = dstH;
        swsParams_.dstFmt = dstFmt;
        swsParams_.flags  = flags;

        swsCtx_ = std::shared_ptr<SwsContext>(ctx, [](SwsContext* p) {
            if (p) sws_freeContext(p);
            });

        return 0;
    }

    int MediaResampler::autoConfigSwsContext(int srcW, int srcH, AVPixelFormat srcFmt,
                                             int dstW, int dstH, AVPixelFormat dstFmt) {
        return configSwsContext(srcW, srcH, srcFmt, dstW, dstH, dstFmt, getBestFlags(srcW, srcH, dstW, dstH));
    }



    int MediaResampler::scaleFrame(AVFrame* srcFrm, AVFrame* dstFrm) {
        if (!srcFrm || !dstFrm) {
            return AVERROR(EINVAL);
        }

        if (!swsCtx_ ||
            srcFrm->width  != swsParams_.srcW   ||
            srcFrm->height != swsParams_.srcH   ||
            srcFrm->format != swsParams_.srcFmt ||
            dstFrm->width  != swsParams_.dstW   ||
            dstFrm->height != swsParams_.dstH   ||
            dstFrm->format != swsParams_.dstFmt) {

            int ret = configSwsContext(srcFrm->width, srcFrm->height, (AVPixelFormat)srcFrm->format,
                                       dstFrm->width, dstFrm->height, (AVPixelFormat)dstFrm->format,
                                       swsParams_.flags);
            if (ret < 0) {
                return ret;
            }
        }

        int ret = sws_scale(swsCtx_.get(), srcFrm->data, srcFrm->linesize, 0, srcFrm->height,
                            dstFrm->data, dstFrm->linesize);
        if (ret <= 0) {
            return AVERROR_EXTERNAL;
        }

        dstFrm->pts                     = srcFrm->pts;
        dstFrm->pkt_dts                 = srcFrm->pkt_dts;
        dstFrm->best_effort_timestamp   = srcFrm->best_effort_timestamp;
        dstFrm->duration                = srcFrm->duration;

        return ret;
    }

    int MediaResampler::configSwrContext(AVChannelLayout outChLayout, AVSampleFormat outSampleFmt, int outSampleRate,
                                         AVChannelLayout inChLayout, AVSampleFormat inSampleFmt, int inSampleRate) {
        if (outSampleRate <= 0 || inSampleRate <= 0 || outSampleFmt == AV_SAMPLE_FMT_NONE || inSampleFmt == AV_SAMPLE_FMT_NONE) {
            return AVERROR(EINVAL);
        }

        if (swrCtx_ && !isSwrParamsChanged(outChLayout, outSampleFmt, outSampleRate,
                                           inChLayout, inSampleFmt, inSampleRate)) {
            return 0;
        }

        resetSwrContext();

        SwrContext* ctx = swr_alloc();
        if (!ctx) {
            return AVERROR(ENOMEM);
        }

        int ret = 0;
        ret |= av_opt_set_chlayout(ctx, "in_chlayout", &inChLayout, 0);
        ret |= av_opt_set_sample_fmt(ctx, "in_sample_fmt", inSampleFmt, 0);
        ret |= av_opt_set_int(ctx, "in_sample_rate", inSampleRate, 0);
        ret |= av_opt_set_chlayout(ctx, "out_chlayout", &outChLayout, 0);
        ret |= av_opt_set_sample_fmt(ctx, "out_sample_fmt", outSampleFmt, 0);
        ret |= av_opt_set_int(ctx, "out_sample_rate", outSampleRate, 0);
        ret |= av_opt_set_int(ctx, "linear_interp", 1, 0);
        ret |= av_opt_set_double(ctx, "cutoff", 0.999, 0);

        if (ret < 0) {
            swr_free(&ctx);
            return ret;
        }

        ret = swr_init(ctx);
        if (ret < 0) {
            swr_free(&ctx);
            return ret;
        }

        av_channel_layout_copy(&swrParams_.inChLayout, &inChLayout);
        swrParams_.inSampleFmt  = inSampleFmt;
        swrParams_.inSampleRate = inSampleRate;
        av_channel_layout_copy(&swrParams_.outChLayout, &outChLayout);
        swrParams_.outSampleFmt  = outSampleFmt;
        swrParams_.outSampleRate = outSampleRate;

        swrCtx_ = std::shared_ptr<SwrContext>(ctx, [](SwrContext* p) {
            if (p) swr_free(&p);
            });

        return 0;
    }



    int MediaResampler::resampleFrame(AVFrame* srcFrm, AVFrame* dstFrm) {
        if (!srcFrm || !dstFrm) {
            return AVERROR(EINVAL);
        }

        if (!swrCtx_ ||
            srcFrm->sample_rate != swrParams_.inSampleRate  ||
            dstFrm->sample_rate != swrParams_.outSampleRate ||
            srcFrm->format      != swrParams_.inSampleFmt   ||
            dstFrm->format      != swrParams_.outSampleFmt  ||
            av_channel_layout_compare(&srcFrm->ch_layout, &swrParams_.inChLayout) != 0 ||
            av_channel_layout_compare(&dstFrm->ch_layout, &swrParams_.outChLayout) != 0) {

            int ret = configSwrContext(dstFrm->ch_layout, (AVSampleFormat)dstFrm->format, dstFrm->sample_rate,
                                       srcFrm->ch_layout, (AVSampleFormat)srcFrm->format, srcFrm->sample_rate);
            if (ret < 0) {
                return ret;
            }
        }

        int ret = swr_convert_frame(swrCtx_.get(), dstFrm, srcFrm);
        if (ret < 0) {
            return ret;
        }

        return dstFrm->nb_samples;
    }

    int MediaResampler::convertAudio(const uint8_t** src, int srcCount,
                                           uint8_t** dst, int dstCount) {
        if (!swrCtx_) {
            return AVERROR(EINVAL);
        }

        return swr_convert(swrCtx_.get(), dst, dstCount, src, srcCount);
    }

    void MediaResampler::resetSwsContext() {
        swsCtx_.reset();
        swsParams_ = SwsParams();
    }

    void MediaResampler::resetSwrContext() {
        swrCtx_.reset();

        av_channel_layout_uninit(&swrParams_.inChLayout);
        av_channel_layout_uninit(&swrParams_.outChLayout);
        swrParams_ = SwrParams();
    }

    void MediaResampler::cleanup() {
        resetSwsContext();
        resetSwrContext();

        av_channel_layout_uninit(&swrParams_.inChLayout);
        av_channel_layout_uninit(&swrParams_.outChLayout);
    }

    bool MediaResampler::isSwsParamsChanged(int srcW, int srcH, AVPixelFormat srcFmt,
                                            int dstW, int dstH, AVPixelFormat dstFmt,
                                            int flags) const {
        return (swsParams_.srcW   != srcW     ||
                swsParams_.srcH   != srcH     ||
                swsParams_.srcFmt != srcFmt   ||
                swsParams_.dstW   != dstW     ||
                swsParams_.dstH   != dstH     ||
                swsParams_.dstFmt != dstFmt   ||
                swsParams_.flags  != flags);
    }

    bool MediaResampler::isSwrParamsChanged(const AVChannelLayout& outChLayout, AVSampleFormat outSampleFmt, int outSampleRate,
                                            const AVChannelLayout& inChLayout, AVSampleFormat inSampleFmt, int inSampleRate) const {
        return (inSampleRate  != swrParams_.inSampleRate    ||
                outSampleRate != swrParams_.outSampleRate   ||
                inSampleFmt   != swrParams_.inSampleFmt     ||
                outSampleFmt  != swrParams_.outSampleFmt    ||
                av_channel_layout_compare(&inChLayout, &swrParams_.inChLayout)   != 0 ||
                av_channel_layout_compare(&outChLayout, &swrParams_.outChLayout) != 0);
    }

    int MediaResampler::getBestFlags(int srcW, int srcH, int dstW, int dstH) const {
        if (dstW > srcW || dstH > srcH) {
            return SWS_BICUBIC;
        }
        else if (dstW * 2 < srcW || dstH * 2 < srcH) {
            return SWS_BICUBIC | SWS_ACCURATE_RND;
        }
        else {
            return SWS_BILINEAR;
        }
    }

} // namespace media