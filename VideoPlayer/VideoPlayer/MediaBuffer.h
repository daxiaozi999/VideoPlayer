#pragma once

#include "Queue.h"
#include "SDK.h"

static constexpr const size_t MAX_VIDEO_PACKETS = 30;
static constexpr const size_t MIN_VIDEO_PACKETS = 15;
static constexpr const size_t MAX_AUDIO_PACKETS = 80;
static constexpr const size_t MIN_AUDIO_PACKETS = 40;
static constexpr const size_t MAX_VIDEO_FRAMES  = 15;
static constexpr const size_t MIN_VIDEO_FRAMES  = 6;
static constexpr const size_t MAX_AUDIO_FRAMES  = 16;
static constexpr const size_t MIN_AUDIO_FRAMES  = 8;

enum class MediaType {
	VideoPacket,
	AudioPacket,
	VideoFrame,
	AudioFrame,
};

class MediaBuffer {
public:
	MediaBuffer(const MediaBuffer&) = delete;
	MediaBuffer& operator=(const MediaBuffer&) = delete;
	MediaBuffer(MediaBuffer&&) = delete;
	MediaBuffer& operator=(MediaBuffer&&) = delete;

	explicit MediaBuffer()
		: vPkts_(MAX_VIDEO_PACKETS, MIN_VIDEO_PACKETS)
		, aPkts_(MAX_AUDIO_PACKETS, MIN_AUDIO_PACKETS)
		, vFrms_(MAX_VIDEO_FRAMES, MIN_VIDEO_FRAMES)
		, aFrms_(MAX_AUDIO_FRAMES, MIN_AUDIO_FRAMES) {

		vPkts_.setClearCallback([](AVPacket* pkt) {
			if (pkt) {
				av_packet_free(&pkt);
			}
			});

		aPkts_.setClearCallback([](AVPacket* pkt) {
			if (pkt) {
				av_packet_free(&pkt);
			}
			});

		vFrms_.setClearCallback([](AVFrame* frm) {
			if (frm) {
				av_frame_free(&frm);
			}
			});

		aFrms_.setClearCallback([](AVFrame* frm) {
			if (frm) {
				av_frame_free(&frm);
			}
			});
	}

	~MediaBuffer() {
		cleanup();
	}

	template<typename T>
	inline bool enqueue(T* item, MediaType type) {
		if (!item) return false;

		switch (type) {
		case MediaType::VideoPacket:
			return vPkts_.enqueue(reinterpret_cast<AVPacket*>(item));
		case MediaType::AudioPacket:
			return aPkts_.enqueue(reinterpret_cast<AVPacket*>(item));
		case MediaType::VideoFrame:
			return vFrms_.enqueue(reinterpret_cast<AVFrame*>(item));
		case MediaType::AudioFrame:
			return aFrms_.enqueue(reinterpret_cast<AVFrame*>(item));
		default:
			return false;
		}
	}

	template<typename T>
	inline T* dequeue(MediaType type) {
		switch (type) {
		case MediaType::VideoPacket:
			return reinterpret_cast<T*>(vPkts_.dequeue());
		case MediaType::AudioPacket:
			return reinterpret_cast<T*>(aPkts_.dequeue());
		case MediaType::VideoFrame:
			return reinterpret_cast<T*>(vFrms_.dequeue());
		case MediaType::AudioFrame:
			return reinterpret_cast<T*>(aFrms_.dequeue());
		default:
			return nullptr;
		}
	}

	size_t size(MediaType type) const {
		switch (type) {
		case MediaType::VideoPacket:
			return vPkts_.size();
		case MediaType::AudioPacket:
			return aPkts_.size();
		case MediaType::VideoFrame:
			return vFrms_.size();
		case MediaType::AudioFrame:
			return aFrms_.size();
		default:
			return 0;
		}
	}

	bool empty(MediaType type) const {
		switch (type) {
		case MediaType::VideoPacket:
			return vPkts_.empty();
		case MediaType::AudioPacket:
			return aPkts_.empty();
		case MediaType::VideoFrame:
			return vFrms_.empty();
		case MediaType::AudioFrame:
			return aFrms_.empty();
		default:
			return true;
		}
	}

	bool full(MediaType type) const {
		switch (type) {
		case MediaType::VideoPacket:
			return vPkts_.full();
		case MediaType::AudioPacket:
			return aPkts_.full();
		case MediaType::VideoFrame:
			return vFrms_.full();
		case MediaType::AudioFrame:
			return aFrms_.full();
		default:
			return true;
		}
	}

	void setLimit(MediaType type, size_t minSize, size_t maxSize) {
		switch (type) {
		case MediaType::VideoPacket:
			vPkts_.setLimit(minSize, maxSize);
			break;
		case MediaType::AudioPacket:
			aPkts_.setLimit(minSize, maxSize);
			break;
		case MediaType::VideoFrame:
			vFrms_.setLimit(minSize, maxSize);
			break;
		case MediaType::AudioFrame:
			aFrms_.setLimit(minSize, maxSize);
			break;
		}
	}

	void abort() {
		vPkts_.abort();
		aPkts_.abort();
		vFrms_.abort();
		aFrms_.abort();
	}

	void resume() {
		vPkts_.resume();
		aPkts_.resume();
		vFrms_.resume();
		aFrms_.resume();
	}

	void cleanup() {
		vPkts_.clear();
		aPkts_.clear();
		vFrms_.clear();
		aFrms_.clear();
	}

private:
	queue::Queue<AVPacket> vPkts_;
	queue::Queue<AVPacket> aPkts_;
	queue::Queue<AVFrame> vFrms_;
	queue::Queue<AVFrame> aFrms_;
};