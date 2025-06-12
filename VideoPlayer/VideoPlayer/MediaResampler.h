#pragma once

#include <memory>
#include "SDK.h"

namespace media {

    class MediaResampler {
    public:
        MediaResampler(const MediaResampler&) = delete;
        MediaResampler& operator=(const MediaResampler&) = delete;
        MediaResampler(MediaResampler&&) = delete;
        MediaResampler& operator=(MediaResampler&&) = delete;

        explicit MediaResampler();
        ~MediaResampler();

        int configSwsContext(int srcW, int srcH, AVPixelFormat srcFmt,
                             int dstW, int dstH, AVPixelFormat dstFmt,
                             int flags = SWS_BICUBIC);

        int autoConfigSwsContext(int srcW, int srcH, AVPixelFormat srcFmt,
                                 int dstW, int dstH, AVPixelFormat dstFmt);

        int scaleFrame(AVFrame* srcFrm, AVFrame* dstFrm);

        int configSwrContext(AVChannelLayout outChLayout, AVSampleFormat outSampleFmt, int outSampleRate,
                             AVChannelLayout inChLayout, AVSampleFormat inSampleFmt, int inSampleRate);

        int resampleFrame(AVFrame* srcFrm, AVFrame* dstFrm);
        int convertAudio(const uint8_t** src, int srcCount, uint8_t** dst, int dstCount);

        void resetSwsContext();
        void resetSwrContext();
        void cleanup();

        bool hasSwsContext()        const { return swsCtx_ != nullptr; }
        bool hasSwrContext()        const { return swrCtx_ != nullptr; }
        SwsContext* getSwsContext() const { return swsCtx_.get(); }
        SwrContext* getSwrContext() const { return swrCtx_.get(); }

    private:
        bool isSwsParamsChanged(int srcW, int srcH, AVPixelFormat srcFormat,
                                int dstW, int dstH, AVPixelFormat dstFormat, int flags) const;
        bool isSwrParamsChanged(const AVChannelLayout& outChLayout, AVSampleFormat outSampleFmt, int outSampleRate,
                                const AVChannelLayout& inChLayout, AVSampleFormat inSampleFmt, int inSampleRate) const;
        int getBestFlags(int srcW, int srcH, int dstW, int dstH) const;

    private:
        struct SwsParams {
            int srcW = 0;
            int srcH = 0;
            AVPixelFormat srcFmt = AV_PIX_FMT_NONE;
            int dstW = 0;
            int dstH = 0;
            AVPixelFormat dstFmt = AV_PIX_FMT_NONE;
            int flags = SWS_BICUBIC;
        } swsParams_;

        struct SwrParams {
            AVChannelLayout inChLayout = {};
            AVSampleFormat inSampleFmt = AV_SAMPLE_FMT_NONE;
            int inSampleRate = 0;
            AVChannelLayout outChLayout = {};
            AVSampleFormat outSampleFmt = AV_SAMPLE_FMT_NONE;
            int outSampleRate = 0;
        } swrParams_;

        std::shared_ptr<SwsContext> swsCtx_;
        std::shared_ptr<SwrContext> swrCtx_;
    };

} // namespace media