#pragma once

#include "CommonDef.h"
#include "module/MyContainer/Buffer.h"

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

extern std::function<void(AVFrame *)> avframedel;
using avframe_ptr = std::unique_ptr<AVFrame, decltype(avframedel)>;

class VideoPlayback;

class VideoDecoder : public std::enable_shared_from_this<VideoDecoder>
{
	using PreviewCallback = std::function<void(std::shared_ptr<VideoCallbackInfo> videoInfo, int64_t currentTime)>;
	using AudioPlayCallback = std::function<void(uint8_t*, uint32_t channelSampleNumber)>;
	using VideoOutputCallback = std::function<void(const VideoCallbackInfo& videoInfo)>;
public:
	VideoDecoder(VideoPlayback* videoPlayback);
	~VideoDecoder();

	int32_t initModule(const char *fileName, const VideoInfo &outVideoInfo, const AudioInfo &outAudioInfo);

	void unInitModule();

	void readFrameFromFile();

	void decoder();
	void decoderVideo(AVPacket* packet);
	void decoderAudio(AVPacket* packet);

	void consume();

	void seekOperate();
	//void decodeVideo();
	//void decodeAudio();

	AVCodecContext *getVideoCodecContext() const { return videoCodecContext; }
	AVCodecContext *getAudioCodecContext() const { return audioCodecContext; }

	void initAudioCallback(AudioPlayCallback audioCallback);

	void initVideoCallBack(PreviewCallback preCallback, VideoOutputCallback videoOutputCallback);

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
	AVFormatContext *formatContext{nullptr};
	AVCodecContext *videoCodecContext{nullptr};
	AVCodecContext *audioCodecContext{nullptr};
	int videoStreamIndex;
	int audioStreamIndex;
	SwsContext *swsContext{nullptr};
	SwrContext *swrContext{nullptr};

	Buffer* m_ptrPCMBuffer{ nullptr };

	PreviewCallback m_previewCallback;
	AudioPlayCallback m_audioPlayCallback;
	VideoOutputCallback m_videoOutputCallback;

	std::thread m_ReadThread;
	std::thread m_ConsumeThread;
	std::thread m_DecoderThread;
	//std::thread m_VideoDecoderThread;
	//std::thread m_AudioDecoderThread;
	// std::mutex m_VideoMutex;
	// std::mutex m_AudioMutex;
	std::mutex m_PauseMutex;
	std::mutex m_queueMutex;		//avpacket���е��������ڶ�ȡ�ͽ����߳�
	std::mutex m_afterDecoderInfoMutex;	//�������е��������ڽ������Ⱦ�߳�
	//std::mutex m_PacketMutex;
	//std::condition_variable m_VideoCV;
	//std::condition_variable m_AudioCV;
	std::condition_variable m_ReadCV;	//���ļ��ж�����������
	std::condition_variable m_queueWaitDecodedCV;	//��������е���������
	std::condition_variable m_queueWaitConsumedCV;	//�����Ѷ��е���������
	std::condition_variable m_PauseCV; // ��ͣʱ����������
	std::condition_variable m_SeekCV;	//seekʱ����������

	//std::queue<AVPacket> m_queueVideoFrame;
	//std::queue<AVPacket> m_queueAudioFrame;
	std::queue<std::pair<AVPacket*, PacketType>> m_queueNeedDecoderPacket;
	std::queue<std::shared_ptr<VideoCallbackInfo>> m_queueVideoInfo;		//�������Ƶ֡���ȴ�����
	std::queue<std::shared_ptr<AudioCallbackInfo>> m_queueAudioInfo;		//�������Ƶ֡���ȴ�����

	std::atomic<bool> m_bSeekState{false};
	std::atomic<double_t> m_dSeekTime{0};

	bool m_bInitState{false};
	bool m_bRunningState{false};
	bool m_bPauseState{false};
	//std::atomic<bool> m_bReadFinished{ false };
	//std::atomic<bool> m_bDecoderFinished{ false };
	//bool m_bFirstVideoPacketAfterSeek{false};
	//bool m_bFirstAudioPacketAfterSeek{false};
	//bool m_bFirstReadedVideoPakcet{false};

	AudioInfo m_stuAudioInfo;
	VideoInfo m_stuVideoInfo;

	//int64_t m_iStartTime{0};
	//int64_t m_iPauseTime{0};
	uint32_t m_uiReadThreadSleepTime{0};
	uint32_t m_uiPerFrameSampleCnt{ 0 };
	//int64_t m_iTotalVideoSeekTime{0}; // ��¼�ܹ������ʱ�䣬���ڼ���������΢��Ϊ��λ
	//int64_t m_iTotalAudioSeekTime{0}; // ��¼�ܹ������ʱ�䣬���ڼ���������΢��Ϊ��λ

	//uint64_t m_uiVideoCurrentTime{0}; // ��¼��ǰ��Ƶʱ�䣬���ڼ���������΢��Ϊ��λ
	//uint64_t m_uiAudioCurrentTime{0}; // ��¼��ǰ��Ƶʱ�䣬���ڼ�������,΢��Ϊ��λ

	VideoPlayback* m_ptrVideoPlayback{ nullptr };
};