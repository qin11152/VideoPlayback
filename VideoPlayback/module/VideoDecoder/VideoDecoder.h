#pragma once

#include "CommonDef.h"

extern "C" {
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

extern std::function<void(AVFrame*)> avframedel;
typedef std::unique_ptr<AVFrame, decltype(avframedel)>  avframe_ptr;

using PreviewCallback = std::function<void(avframe_ptr framePtr,int64_t currentTime)>;
using AudioPlayCallback = std::function<void(uint8_t**, uint32_t channelSampleNumber)>;

class VideoDecoder
{
public:
	VideoDecoder();
	~VideoDecoder();

	int32_t initModule(const char* fileName, const VideoInfo& outVideoInfo,const AudioInfo& outAudioInfo);

	void unInitModule();

	void readFrameFromFile();

	void decodeVideo();
	void decodeAudio();

	AVCodecContext* getVideoCodecContext() const { return videoCodecContext; }
	AVCodecContext* getAudioCodecContext() const { return audioCodecContext; }

	void initCallBack(PreviewCallback preCallback, AudioPlayCallback audioCallback);

	void startDecoder();

	void pauseDecoder();

	void resumeDecoder();

	//************************************
	// Method:    seekTo
	// FullName:  VideoDecoder::seekTo
	// Access:    public 
	// Returns:   void
	// Qualifier:
	// Parameter: int64_t time
	// brief:
	//************************************
	void seekTo(double_t time);

	void clearBuffer();

private:
	AVFormatContext* formatContext{ nullptr };
	AVCodecContext* videoCodecContext{ nullptr };
	AVCodecContext* audioCodecContext{ nullptr };
	int videoStreamIndex;
	int audioStreamIndex;
	SwsContext* swsContext{ nullptr };
	SwrContext* swrContext{ nullptr };

	PreviewCallback previewCallback;
	AudioPlayCallback audioPlayCallback;

	std::thread m_ReadThread;
	std::thread m_VideoDecoderThread;
	std::thread m_AudioDecoderThread;
	//std::mutex m_VideoMutex;
	//std::mutex m_AudioMutex;
	std::mutex m_PauseMutex;
	std::mutex m_PacketMutex;
	std::condition_variable m_VideoCV;
	std::condition_variable m_AudioCV;
	std::condition_variable m_ReadCV;

	std::condition_variable m_PauseCV;		//暂停时所有线程暂停工作

	std::queue<AVPacket> m_queueVideoFrame;
	std::queue<AVPacket> m_queueAudioFrame;

	std::atomic<bool> m_bSeekState{ false };
	std::atomic<double_t> m_iSeekTime{ 0 };

	bool m_bInitState{ false };
	bool m_bRunningState{ false };
	bool m_bPauseState{ false };
	bool m_bFirstVideoPacketAfterSeek{ false };
	bool m_bFirstAudioPacketAfterSeek{ false };
	bool m_bFirstReadedVideoPakcet{ false };

	AudioInfo m_stuAudioInfo;
	VideoInfo m_stuVideoInfo;

	int64_t m_iStartTime{ 0 };
	int64_t m_iPauseTime{ 0 };
	uint32_t m_uiReadThreadSleepTime{ 0 };
	int64_t m_iTotalVideoSeekTime{ 0 };		//记录总共快进的时间，用于计算快进量，微妙为单位
	int64_t m_iTotalAudioSeekTime{ 0 };		//记录总共快进的时间，用于计算快进量，微妙为单位

	uint64_t m_uiVideoCurrentTime{ 0 };		//记录当前视频时间，用于计算快进量，微妙为单位
	uint64_t m_uiAudioCurrentTime{ 0 };		//记录当前音频时间，用于计算快进量,微妙为单位
};

