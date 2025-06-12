#pragma once

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
	#include <libavfilter/avfilter.h>
	#include <libavfilter/buffersink.h>
	#include <libavfilter/buffersrc.h>
	#include <libavutil/fifo.h>
	#include <libavutil/time.h>
	#include <libavutil/opt.h>
	#include <libavutil/channel_layout.h>
	#include <libavutil/samplefmt.h>
	#include <libavutil/bprint.h>
	#include <libavutil/buffer.h>
	#include <libavutil/error.h>
	#include <libswresample/swresample.h>
	#include <libavutil/imgutils.h>
	#include <libswscale/swscale.h>
	#include <libavutil/pixdesc.h>
}

extern "C" {
	#include <SDL3\SDL.h>
	#include <SDL3\SDL_audio.h>
	#include <SDL3\SDL_error.h>
}