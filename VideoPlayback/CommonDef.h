#pragma once


#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
	#ifdef MY_DLL_EXPORT
		#define MY_EXPORT __declspec(dllexport)
	#else
		#define MY_EXPORT
	#endif
#else
	#define MY_EXPORT __attribute__((visibility("default")))
#endif

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
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#ifndef MY_DLL_EXPORT 
	#include <QDebug>
	#include "ui/MyTipDialog/MyTipDialog.h"
#endif

#include "module/LogModule/Log.h"
#include "module/ThreadPool/ThreadPool.h"
#include "module/MyContainer/MyQueue.h"

#include <atomic>
#include <mutex>
#include <memory>
#include <deque>
#include <queue>
#include <thread>
#include <algorithm>
#include <functional>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

#define BlackMagicEnabled

constexpr int kOutputVideoWidth = 3840;
constexpr int kOutputVideoHeight = 2160;
constexpr int kOutputVideoFormat = AV_PIX_FMT_UYVY422;
constexpr int kOutputAudioChannels = 2;
constexpr int kAtomOutputAudioChannel = 1;
constexpr int kOutputAudioSampleRate = 48000;
constexpr int kOutputAudioSamplePerChannel = 1024;
constexpr int kOutputAudioBitDepth = 16;
constexpr int kOutputAudioFormat = AV_SAMPLE_FMT_S16;

//constexpr int kSDIOutputFormat = bmdModeHD1080i50;
constexpr int kSDIOutputFormat = bmdMode4K2160p25;

constexpr int kBufferWaterLevel = 50;
constexpr int kAfterDecoderCachedCnt = 50;

constexpr int kmicroSecondsPerSecond = 1000000;
constexpr int kmilliSecondsPerSecond = 1000;

constexpr double kdEpsilon = 1e-9;

enum class PacketType
{
	None = 0,
	Video,
	Audio,
};

enum class MediaType
{
	Invalid,
	Video,
	Audio,
	VideoAndAudio,
};

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
	MediaType mediaType{ MediaType::Invalid };
};

/*!
 * \class VideoInfo
 *
 * \brief 解码后待使用的视频参数，宽高、持续时间、图片格式
 *
 * \author DELL
 * \date 2024/12/10 14:28
 */
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
#ifndef MY_DLL_EXPORT 
	Q_DECLARE_METATYPE(VideoInfo);
#endif
/*!
 * \class AudioInfo
 *
 * \brief 解码后待使用的音频的参数，声道数、采样率、位深、每个通道采样数量、持续时间等
 *
 * \author DELL
 * \date 2024/12/10 14:29
 */
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

/*!
 * \class DecodedImageInfo
 *
 * \brief 解码后的yuv数据存储结构体，包含yuv数据，宽高、图片格式、在视频中的pts等
 *
 * \author DELL
 * \date 2024/12/10 14:30
 */
struct DecodedImageInfo
{
	uint32_t width;
	uint32_t height;
	uint32_t dataSize;
	AVPixelFormat videoFormat;
	uint8_t* yuvData{ nullptr };
	double m_dPts{ 0.0 };
	bool m_bRefresh{ false };		//是否需要立即刷新到界面
	~DecodedImageInfo()
	{
		if (yuvData)
		{
			delete[] yuvData;
			yuvData = nullptr;
		}
	}
};

struct DecodedAudioInfo
{
	uint32_t m_uiChannelCnt;
	uint32_t m_uiNumberSamples;
	uint32_t m_uiSampleRate;
	uint32_t m_uiPCMLength;
	uint8_t* m_ptrPCMData{ nullptr };
	AVSampleFormat m_AudioFormat{ AVSampleFormat::AV_SAMPLE_FMT_NONE };
	double m_dPts{ 0.0 };

	~DecodedAudioInfo()
	{
		if (m_ptrPCMData)
		{
			delete[]m_ptrPCMData;
			m_ptrPCMData = nullptr;
		}
	}
};

/*!
 * \class AudioCallbackInfo
 *
 * \brief 解码后的pcm数据存储结构体，包含pcm数据，音频长度等
 *
 * \author DELL
 * \date 2024/12/10 14:39
 */
struct AudioCallbackInfo
{
	uint32_t m_ulPCMLength{ 0 };
	uint8_t* m_pPCMData{ nullptr };
	std::vector<uint8_t*> m_vecPcmData;
	bool m_bAtom{ false };
	~AudioCallbackInfo()
	{
		if (m_pPCMData)
		{
			delete[]m_pPCMData;
			m_pPCMData = nullptr;
		}
		for (auto& item : m_vecPcmData)
		{
			if (item)
			{
				delete[]item;
			}
		}
	}
};
#ifndef MY_DLL_EXPORT 
	Q_DECLARE_METATYPE(DecodedImageInfo);
#endif
struct PacketWaitDecoded
{
	AVPacket* packet{ nullptr };
	PacketType type{PacketType::None };

	PacketWaitDecoded(AVPacket* p, PacketType t) : packet(p), type(t) {}

	~PacketWaitDecoded()
	{
		if (packet)
		{
			av_packet_free(&packet);
			packet = nullptr;
		}
	}
};

/*!
 * \class VideoReaderInitedInfo
 *
 * \brief 文件读取类初始化所需结构体，包含文件名，音视频输出结构体，待编码队列等
 *
 * \author DELL
 * \date 2024/12/10 14:45
 */
struct VideoReaderInitedInfo
{
	std::string m_strFileName{ "" };
	AVHWDeviceType m_eDeviceType{ AV_HWDEVICE_TYPE_NONE };
	bool m_bAtom{ false };
	VideoInfo outVideoInfo;
	AudioInfo outAudioInfo;
	std::shared_ptr<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>> ptrPacketQueue;
};

/*!
 * \class DecoderInitedInfo
 *
 * \brief 初始化编码器的入参，包含AVFormatContext，编码参数，音视频输出参数，是否硬编，音视频index，待编码avoacket队列
 *
 * \author DELL
 * \date 2024/12/10 14:21
 */
struct DecoderInitedInfo
{
	AVFormatContext* formatContext{ nullptr };
	AVHWDeviceType m_eDeviceType{ AV_HWDEVICE_TYPE_NONE };
	AVCodecParameters* videoCodecParameters{ nullptr };
	AVCodecParameters* audioCodecParameters{ nullptr };
	AVCodec* videoCodec{ nullptr };
	AVCodec* audioCodec{ nullptr };
	std::shared_ptr<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>> ptrPacketQueue;

	//atom相关的内容
	bool m_bAtom{ false };
	AVFormatContext* atomVideoFormatContext{ nullptr };
	std::vector<AVFormatContext*> vecAudioFormatContext;
	AVCodecParameters* atomVideoCodecParameters{ nullptr };
	std::vector<std::pair<AVCodecParameters*,int32_t>> vecAtomAudioCodecParameters;
	AVCodec* atomVideoCodec{ nullptr };
	std::vector<AVCodec*> vecAtomAudioCodec;
	std::shared_ptr<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>> ptrAtomVideoPacketQueue;
	std::vector < std::shared_ptr<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>>> vecAtomAudioPacketQueue;

	VideoInfo outVideoInfo;
	int iVideoIndex{ -1 };
	AudioInfo outAudioInfo;
	int iAudioIndex{ -1 };
};

struct DataHandlerInitedInfo
{
	uint32_t uiNeedSleepTime{ 0 };
	uint32_t uiPerFrameSampleCnt{ 0 };
};

enum class SeekType
{
	SeekInvalid = 0,
	SeekAbsolute,	//按照绝对位置seek
	SeekFrameStep,	//按照帧步进
};

struct SeekParams
{
	double m_dSeekTime{ 0.0 };
	double m_dDstPts{ 0.0 };
	int m_iSeekCnt{ 0 };
	int direction{ 0 };
	SeekType seekType{ SeekType::SeekInvalid };
};

enum class SyncType
{
	SYNC_AUDIO_MASTER, /* default choice */
	SYNC_VIDEO_MASTER,
	SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

enum class ErrorCode
{
	NoError=0,
	AllocateContextError,
	OpenInputError,
	FindStreamInfoError,
	AllocateRsampleError,
};
