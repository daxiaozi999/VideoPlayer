#pragma once

#include <memory>
#include "SDK.h"

namespace media {

    class MediaDecoder {
    public:
        MediaDecoder(const MediaDecoder&) = delete;
        MediaDecoder& operator=(const MediaDecoder&) = delete;
        MediaDecoder(MediaDecoder&&) = delete;
        MediaDecoder& operator=(MediaDecoder&&) = delete;

        explicit MediaDecoder();
        ~MediaDecoder();

        int openDecoder(AVFormatContext* fmtCtx, int streamIndex, bool hwAccelEnable);
        int openDecoder(AVFormatContext* fmtCtx, int streamIndex, bool hwAccelEnable, unsigned int threads);

        void flushAllDecoder();
        void resetVideoDecoder();
        void resetAudioDecoder();
        void cleanup();

        bool hasVideoDecoder()  const { return vDecCtx_ != nullptr; }
        bool hasAudioDecoder()  const { return aDecCtx_ != nullptr; }
        bool isHwAccelEnabled() const { return hwAccelEnabled_; }

        AVCodecContext* getVideoDecoder() const { return vDecCtx_.get(); }
        AVCodecContext* getAudioDecoder() const { return aDecCtx_.get(); }

    private:
        int openInternalDecoder(AVFormatContext* fmtCtx, int streamIndex,
                                const AVCodec*& dec, std::shared_ptr<AVCodecContext>& decCtx,
                                unsigned int threads);

    private:
        bool hwAccelEnabled_;
        const AVCodec* vDec_;
        const AVCodec* aDec_;
        std::shared_ptr<AVCodecContext> vDecCtx_;
        std::shared_ptr<AVCodecContext> aDecCtx_;
    };

} // namespace media