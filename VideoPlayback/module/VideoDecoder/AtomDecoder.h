#pragma once

#include "CommonDef.h"
#include "module/VideoReader/VideoReader.h"
#include "module/VideoDecoder/VideoDecoderBase.h"

class MY_EXPORT AtomDecoder : public VideoDecoderBase
{
public:

	AtomDecoder(std::vector<std::shared_ptr<VideoReader>> vecVideoReader);
	~AtomDecoder();

	//************************************
	// Method:    initModule
	// FullName:  HardDecoder::initModule
	// Access:    public 
	// Returns:   int32_t 0:success -1:failed
	// Qualifier:
	// Parameter: const DecoderInitedInfo & info，入参
	// brief: 初始化解码器，开启解码工作线程
	//************************************
	int32_t initModule(const DecoderInitedInfo& info, DataHandlerInitedInfo& dataHandlerInfo)override;


	//************************************
	// Method:    uninitModule
	// FullName:  HardDecoder::uninitModule
	// Access:    public 
	// Returns:   int32_t
	// brief: 反初始化，结束编码线程，释放资源
	// Qualifier:
	//************************************
	int32_t uninitModule()override;

	int32_t addAtomVideoPacketQueue(std::shared_ptr<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptrPacketQueue)override;

	int32_t addAtomAudioPacketQueue(std::shared_ptr<std::vector<std::shared_ptr<Buffer>>> vecBuffer)override;

	int32_t seekTo(double_t seekTime)override;

	void registerFinishedCallback(DecoderFinishedCallback callback)override;

private:
	void readVideoPacket();

	void readAudioPacket();

	//************************************
	// Method:    decodeVideo
	// FullName:  HardDecoder::decodeVideo
	// Access:    private 
	// Returns:   void
	// Qualifier:
	// brief: 视频包解码函数
	// Parameter: PacketWaitDecoded & packet待解码的包，包含数据和类型
	//************************************
	void decodeVideo(std::shared_ptr<PacketWaitDecoded> packet)override;

	//************************************
	// Method:    decodeAudio
	// FullName:  HardDecoder::decodeAudio
	// Access:    private 
	// Returns:   void
	// Qualifier:
	// brief: 音频包解码函数
	// Parameter: PacketWaitDecoded & packet 待解码的包，包含数据和类型
	//************************************
	void decodeAudio(std::shared_ptr<PacketWaitDecoded> packet, int index)override;

	//************************************
	// Method:    initVideoDecoder
	// FullName:  HardDecoder::initVideoDecoder
	// Access:    private 
	// Returns:   int32_t
	// Qualifier:
	// Parameter: const DecoderInitedInfo & info
	//************************************
	int32_t initVideoDecoder(const DecoderInitedInfo& info);
	int32_t initAudioDecoder(const DecoderInitedInfo& info);

	void flushDecoder();

	void seekOperate();

private:
	std::thread m_VideoDecodeThread;
	std::thread m_AudioDecodeThread;

	std::vector< std::shared_ptr<VideoReader>> m_vecVideoReader;
    AVFormatContext *formatContext{nullptr};
    AVCodecContext *videoCodecContext{nullptr};

    std::vector<std::pair<AVCodecContext*,int32_t>> m_vecAudioCodecContext;
    std::vector<AVFormatContext*> m_vecAudioFormatContext;
	SwsContext* swsContext{ nullptr };
	std::vector<SwrContext*> m_vecSwrContext;
	int m_iVideoStreamIndex;
	double m_dFrameDuration{ 1.0 };

	bool m_bInitState{ false };
	bool m_bRunningState{ false };

	bool m_bPauseState = false;
	std::mutex m_PauseMutex;
	std::condition_variable m_PauseCV;

	bool m_bDecoderedFinished{ false };

	std::atomic<bool> m_bSeekState{ false };
	std::atomic<double_t> m_dSeekTime{ 0 };

	AudioInfo m_stuAudioInfo;
	VideoInfo m_stuVideoInfo;

	uint32_t m_uiReadThreadSleepTime{ 0 };
	uint32_t m_uiPerFrameSampleCnt{ 0 };

	//待解码的包队列
	std::shared_ptr<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>> m_ptrQueNeedDecodedVideoPacket;
	//待解码的音频包队列
	std::vector <std::shared_ptr<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>>> m_vecQueueNeedDecodedAudioPacket;

	//一个解码器可能有多个消耗者，对应多个队列
	std::mutex m_PcmBufferAddMutex;
	std::mutex m_VideoQueueAddMutex;
	std::vector<std::shared_ptr<std::vector<std::shared_ptr<Buffer>>>> m_vecPCMBuffer;	//解码后的音频数据队列
	std::vector<std::shared_ptr <MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>>> m_vecQueDecodedVideoPacket;	//解码后的视频数据队列
    
};