#pragma once

#include "CommonDef.h"
#include "module/AtomDecoder/Buffer.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <atomic>
#include <mutex>
#include <memory>
#include <queue>
#include <thread>
#include <functional>
#include <condition_variable>

class HardDecoder
{
public:
	HardDecoder();
	~HardDecoder();

	int32_t initModule(const char* fileName, const VideoInfo& outVideoInfo, const AudioInfo& outAudioInfo, const enum AVHWDeviceType type);

	void unInitModule();

	void startDecoder();

private:
	void decoder();
	void decoderVideo(AVPacket* packet);
	void decoderAudio(AVPacket* packet);

	void readFrameFromFile();

	AVFormatContext* formatContext{ nullptr };
	AVBufferRef* m_ptrHWDeviceCtx;
	enum AVPixelFormat m_hwPixFormat;
	AVCodecContext* videoCodecContext{ nullptr };
	AVCodecContext* audioCodecContext{ nullptr };
	int videoStreamIndex;
	int audioStreamIndex;
	SwsContext* swsContext{ nullptr };
	SwrContext* swrContext{ nullptr };

	Buffer* m_ptrPCMBuffer{ nullptr };

	std::thread m_ReadThread;
	std::thread m_ConsumeThread;
	std::thread m_DecoderThread;

	std::mutex m_PauseMutex;
	std::mutex m_queueMutex;		//avpacket队列的锁，用于读取和解码线程
	std::mutex m_afterDecoderInfoMutex;	//编码后队列的锁，用于解码和渲染线程

	std::condition_variable m_ReadCV;	//从文件中读的条件变量
	std::condition_variable m_queueWaitDecodedCV;	//待解码队列的条件变量
	std::condition_variable m_queueWaitConsumedCV;	//待消费队列的条件变量
	std::condition_variable m_PauseCV; // 暂停时的条件变量
	std::condition_variable m_SeekCV;	//seek时的条件变量


	std::queue<std::pair<AVPacket*, PacketType>> m_queueNeedDecoderPacket;
	std::queue<std::shared_ptr<VideoCallbackInfo>> m_queueVideoInfo;		//解码的视频帧，等待消费
	std::queue<std::shared_ptr<AudioCallbackInfo>> m_queueAudioInfo;		//解码的音频帧，等待消费

	std::atomic<bool> m_bSeekState{ false };
	std::atomic<double_t> m_dSeekTime{ 0 };

	bool m_bInitState{ false };
	bool m_bRunningState{ false };
	bool m_bPauseState{ false };

	AudioInfo m_stuAudioInfo;
	VideoInfo m_stuVideoInfo;

	uint32_t m_uiReadThreadSleepTime{ 0 };
	uint32_t m_uiPerFrameSampleCnt{ 0 };

};

