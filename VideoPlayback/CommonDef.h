#pragma once

#if defined(WIN32)
#include "stdafx.h"
#include "com_ptr.h"
#include "DeckLinkAPI_h.h"
#elif defined(__linux__)
#include "DeckLinkAPI.h"
#endif

extern "C"
{
#include <ffmpeg/libavutil/pixfmt.h>
#include <ffmpeg/libavutil/samplefmt.h>
}
#include <QDebug>

#include "module/LogModule/Log.h"

#include <cstdint>
#include <fstream>

constexpr int kOutputVideoWidth = 1920;
constexpr int kOutputVideoHeight = 1080;
constexpr int kOutputVideoFormat = AV_PIX_FMT_YUV422P;
constexpr int kOutputAudioChannels = 2;
constexpr int kOutputAudioSampleRate = 48000;
constexpr int kOutputAudioSamplePerChannel = 1024;
constexpr int kOutputAudioBitDepth = 16;
constexpr int kOutputAudioFormat = AV_SAMPLE_FMT_S16;

constexpr int kSDIOutputFormat = bmdModeHD1080i50;

constexpr int kBufferWaterLevel = 20;

constexpr int kmicroSecondsPerSecond = 1000000;
constexpr int kmilliSecondsPerSecond = 1000;


struct MediaInfo
{
	int width{ 0 };
	int height{ 0 };
	double fps{ 0 };
	double duration{ 0 };
	int bitrate{ 0 };
	int frameCount{ 0 };
	int audioChannels{ 0 };
	int audioSampleRate{ 0 };
	int bitDepth{ 0 };
	int64_t audioBitrate{ 0 };
	int audioDuration{ 0 };
	int audioFrameCount{ 0 };
	AVPixelFormat videoFormat{ AV_PIX_FMT_NONE };
	AVSampleFormat audioFormat{ AV_SAMPLE_FMT_NONE };
};

struct VideoInfo
{
	int width{ 0 };
	int height{ 0 };
	double fps{ 0 };
	double duration{ 0 };
	int bitrate{ 0 };
	int frameCount{ 0 };
	AVPixelFormat videoFormat{ AV_PIX_FMT_NONE };
};

Q_DECLARE_METATYPE(VideoInfo);

struct AudioInfo
{
	int audioChannels{ 0 };
	int audioSampleRate{ 0 };
	int bitDepth{ 0 };
	int64_t audioBitrate{ 0 };
	int samplePerChannel{ 0 };
	int audioDuration{ 0 };
	int audioFrameCount{ 0 };
	AVSampleFormat audioFormat{ AV_SAMPLE_FMT_NONE };
};

struct VideoCallbackInfo
{
	uint32_t width;
	uint32_t height;
	uint32_t dataSize;
	AVPixelFormat videoFormat;
	uint8_t* yuvData{ nullptr };
	~VideoCallbackInfo()
	{
		if (yuvData)
		{
			delete[] yuvData;
			yuvData = nullptr;
		}
	}
};

Q_DECLARE_METATYPE(VideoCallbackInfo);

enum class ErrorCode
{
	NoError=0,
	AllocateContextError,
	OpenInputError,
	FindStreamInfoError,
	AllocateRsampleError,
};
