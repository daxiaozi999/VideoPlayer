#pragma once

#include <vector>
#include <type_traits>
#include <unordered_map>
#include "FFmpeg.h"
#include "MediaQueue.h"

namespace media {

    enum MediaType {
        VIDEO,
        AUDIO
    };

    enum MediaState {
        DEMUXING,
        DECODING,
        ENCODING,
    };

    struct MediaLimit {
        MediaType type;
        MediaState state;
        size_t minSize;
        size_t maxSize;
    };

    static const MediaLimit MediaLimit_Preset[] = {
        {VIDEO, DEMUXING, 30ULL,  60ULL},
        {AUDIO, DEMUXING, 50ULL,  100ULL},
        {VIDEO, DECODING, 30ULL,  60ULL},
        {AUDIO, DECODING, 500ULL, 1000ULL},
        {VIDEO, ENCODING, 30ULL,  60ULL},
        {AUDIO, ENCODING, 500ULL, 1000ULL},
    };

} // namespace media

class MediaBuffer {
public:
    MediaBuffer(const MediaBuffer&) = delete;
    MediaBuffer& operator=(const MediaBuffer&) = delete;
    MediaBuffer(MediaBuffer&&) = delete;
    MediaBuffer& operator=(MediaBuffer&&) = delete;

    MediaBuffer() {
        auto createQueue = [this](media::MediaType t, media::MediaState s, size_t l, size_t r) {
            switch (s) {
            case media::DEMUXING:
            case media::ENCODING: {
                media::MediaQueue<AVPacket> q(l, r);
                q.setClearCallback([](AVPacket* p) {
                    if (p) {
                        av_packet_free(&p);
                    }
                    });
                switch (t) {
                case media::VIDEO:
                    videoPackets_[s].push_back(std::move(q));
                    break;
                case media::AUDIO:
                    audioPackets_[s].push_back(std::move(q));
                    break;
                }
                break;
            }
            case media::DECODING: {
                media::MediaQueue<AVFrame> q(l, r);
                q.setClearCallback([](AVFrame* f) {
                    if (f) {
                        av_frame_free(&f);
                    }
                    });
                switch (t) {
                case media::VIDEO:
                    videoFrames_[s].push_back(std::move(q));
                    break;
                case media::AUDIO:
                    audioFrames_[s].push_back(std::move(q));
                    break;
                }
                break;
            }
            }
            };

        createQueue(media::VIDEO, media::DEMUXING, media::MediaLimit_Preset[0].minSize, media::MediaLimit_Preset[0].maxSize);
        createQueue(media::AUDIO, media::DEMUXING, media::MediaLimit_Preset[1].minSize, media::MediaLimit_Preset[1].maxSize);
        createQueue(media::VIDEO, media::DECODING, media::MediaLimit_Preset[2].minSize, media::MediaLimit_Preset[2].maxSize);
        createQueue(media::AUDIO, media::DECODING, media::MediaLimit_Preset[3].minSize, media::MediaLimit_Preset[3].maxSize);
    }

    ~MediaBuffer() {
        lock();
        clear();
    }

    template<typename T, media::MediaType Tt, media::MediaState Ts>
    bool enqueue(T* item, size_t index = 0) {
        if (!item) {
            return false;
        }

        switch (Ts) {
        case media::DEMUXING:
        case media::ENCODING: {
            if constexpr (std::is_same_v<T, AVPacket>) {
                switch (Tt) {
                case media::VIDEO: {
                    auto it = videoPackets_.find(Ts);
                    if (it != videoPackets_.end() && index < it->second.size()) {
                        return it->second[index].enqueue(item);
                    }
                    break;
                }
                case media::AUDIO: {
                    auto it = audioPackets_.find(Ts);
                    if (it != audioPackets_.end() && index < it->second.size()) {
                        return it->second[index].enqueue(item);
                    }
                    break;
                }
                }
            }
            break;
        }
        case media::DECODING: {
            if constexpr (std::is_same_v<T, AVFrame>) {
                switch (Tt) {
                case media::VIDEO: {
                    auto it = videoFrames_.find(Ts);
                    if (it != videoFrames_.end() && index < it->second.size()) {
                        return it->second[index].enqueue(item);
                    }
                    break;
                }
                case media::AUDIO: {
                    auto it = audioFrames_.find(Ts);
                    if (it != audioFrames_.end() && index < it->second.size()) {
                        return it->second[index].enqueue(item);
                    }
                    break;
                }
                }
            }
            break;
        }
        }

        return false;
    }

    template<media::MediaType Tt, media::MediaState Ts>
    void* dequeue(size_t index = 0) {
        switch (Ts) {
        case media::DEMUXING:
        case media::ENCODING: {
            switch (Tt) {
            case media::VIDEO: {
                auto it = videoPackets_.find(Ts);
                if (it != videoPackets_.end() && index < it->second.size()) {
                    return it->second[index].dequeue();
                }
                break;
            }
            case media::AUDIO: {
                auto it = audioPackets_.find(Ts);
                if (it != audioPackets_.end() && index < it->second.size()) {
                    return it->second[index].dequeue();
                }
                break;
            }
            }
            break;
        }
        case media::DECODING: {
            switch (Tt) {
            case media::VIDEO: {
                auto it = videoFrames_.find(Ts);
                if (it != videoFrames_.end() && index < it->second.size()) {
                    return it->second[index].dequeue();
                }
                break;
            }
            case media::AUDIO: {
                auto it = audioFrames_.find(Ts);
                if (it != audioFrames_.end() && index < it->second.size()) {
                    return it->second[index].dequeue();
                }
                break;
            }
            }
            break;
        }
        }

        return nullptr;
    }

    template<media::MediaType Tt, media::MediaState Ts>
    size_t size(size_t index = 0) const {
        switch (Ts) {
        case media::DEMUXING:
        case media::ENCODING: {
            switch (Tt) {
            case media::VIDEO: {
                auto it = videoPackets_.find(Ts);
                if (it != videoPackets_.end() && index < it->second.size()) {
                    return it->second[index].size();
                }
                break;
            }
            case media::AUDIO: {
                auto it = audioPackets_.find(Ts);
                if (it != audioPackets_.end() && index < it->second.size()) {
                    return it->second[index].size();
                }
                break;
            }
            }
            break;
        }
        case media::DECODING: {
            switch (Tt) {
            case media::VIDEO: {
                auto it = videoFrames_.find(Ts);
                if (it != videoFrames_.end() && index < it->second.size()) {
                    return it->second[index].size();
                }
                break;
            }
            case media::AUDIO: {
                auto it = audioFrames_.find(Ts);
                if (it != audioFrames_.end() && index < it->second.size()) {
                    return it->second[index].size();
                }
                break;
            }
            }
            break;
        }
        }

        return 0;
    }

    template<media::MediaType Tt, media::MediaState Ts>
    bool empty(size_t index = 0) const {
        switch (Ts) {
        case media::DEMUXING:
        case media::ENCODING: {
            switch (Tt) {
            case media::VIDEO: {
                auto it = videoPackets_.find(Ts);
                if (it != videoPackets_.end() && index < it->second.size()) {
                    return it->second[index].empty();
                }
                break;
            }
            case media::AUDIO: {
                auto it = audioPackets_.find(Ts);
                if (it != audioPackets_.end() && index < it->second.size()) {
                    return it->second[index].empty();
                }
                break;
            }
            }
            break;
        }
        case media::DECODING: {
            switch (Tt) {
            case media::VIDEO: {
                auto it = videoFrames_.find(Ts);
                if (it != videoFrames_.end() && index < it->second.size()) {
                    return it->second[index].empty();
                }
                break;
            }
            case media::AUDIO: {
                auto it = audioFrames_.find(Ts);
                if (it != audioFrames_.end() && index < it->second.size()) {
                    return it->second[index].empty();
                }
                break;
            }
            }
            break;
        }
        }

        return true;
    }

    template<media::MediaType Tt, media::MediaState Ts>
    bool full(size_t index = 0) const {
        switch (Ts) {
        case media::DEMUXING:
        case media::ENCODING: {
            switch (Tt) {
            case media::VIDEO: {
                auto it = videoPackets_.find(Ts);
                if (it != videoPackets_.end() && index < it->second.size()) {
                    return it->second[index].full();
                }
                break;
            }
            case media::AUDIO: {
                auto it = audioPackets_.find(Ts);
                if (it != audioPackets_.end() && index < it->second.size()) {
                    return it->second[index].full();
                }
                break;
            }
            }
            break;
        }
        case media::DECODING: {
            switch (Tt) {
            case media::VIDEO: {
                auto it = videoFrames_.find(Ts);
                if (it != videoFrames_.end() && index < it->second.size()) {
                    return it->second[index].full();
                }
                break;
            }
            case media::AUDIO: {
                auto it = audioFrames_.find(Ts);
                if (it != audioFrames_.end() && index < it->second.size()) {
                    return it->second[index].full();
                }
                break;
            }
            }
            break;
        }
        }

        return false;
    }

    template<media::MediaType Tt, media::MediaState Ts>
    void setLimit(size_t l, size_t r, size_t index = 0) {
        switch (Ts) {
        case media::DEMUXING:
        case media::ENCODING: {
            switch (Tt) {
            case media::VIDEO: {
                auto it = videoPackets_.find(Ts);
                if (it != videoPackets_.end() && index < it->second.size()) {
                    it->second[index].setLimit(l, r);
                }
                break;
            }
            case media::AUDIO: {
                auto it = audioPackets_.find(Ts);
                if (it != audioPackets_.end() && index < it->second.size()) {
                    it->second[index].setLimit(l, r);
                }
                break;
            }
            }
            break;
        }
        case media::DECODING: {
            switch (Tt) {
            case media::VIDEO: {
                auto it = videoFrames_.find(Ts);
                if (it != videoFrames_.end() && index < it->second.size()) {
                    it->second[index].setLimit(l, r);
                }
                break;
            }
            case media::AUDIO: {
                auto it = audioFrames_.find(Ts);
                if (it != audioFrames_.end() && index < it->second.size()) {
                    it->second[index].setLimit(l, r);
                }
                break;
            }
            }
            break;
        }
        }
    }

    void lock() {
        for (auto& v : videoPackets_) {
            for (auto& q : v.second) {
                q.lock();
            }
        }

        for (auto& v : audioPackets_) {
            for (auto& q : v.second) {
                q.lock();
            }
        }

        for (auto& v : videoFrames_) {
            for (auto& q : v.second) {
                q.lock();
            }
        }

        for (auto& v : audioFrames_) {
            for (auto& q : v.second) {
                q.lock();
            }
        }
    }

    void unlock() {
        for (auto& v : videoPackets_) {
            for (auto& q : v.second) {
                q.unlock();
            }
        }

        for (auto& v : audioPackets_) {
            for (auto& q : v.second) {
                q.unlock();
            }
        }

        for (auto& v : videoFrames_) {
            for (auto& q : v.second) {
                q.unlock();
            }
        }

        for (auto& v : audioFrames_) {
            for (auto& q : v.second) {
                q.unlock();
            }
        }
    }

    void clear() {
        for (auto& v : videoPackets_) {
            for (auto& q : v.second) {
                q.clear();
            }
        }

        for (auto& v : audioPackets_) {
            for (auto& q : v.second) {
                q.clear();
            }
        }

        for (auto& v : videoFrames_) {
            for (auto& q : v.second) {
                q.clear();
            }
        }

        for (auto& v : audioFrames_) {
            for (auto& q : v.second) {
                q.clear();
            }
        }
    }

private:
    std::unordered_map<media::MediaState, std::vector<media::MediaQueue<AVPacket>>> videoPackets_;
    std::unordered_map<media::MediaState, std::vector<media::MediaQueue<AVPacket>>> audioPackets_;
    std::unordered_map<media::MediaState, std::vector<media::MediaQueue<AVFrame>>> videoFrames_;
    std::unordered_map<media::MediaState, std::vector<media::MediaQueue<AVFrame>>> audioFrames_;
};