#pragma once

#include "CommonDef.h"
#include "module/MyContainer/Buffer.h"
#include "module/VideoReader/VideoReader.h"
#include "module/VideoDecoder/VideoDecoderBase.h"

class MY_EXPORT HardDecoder : public VideoDecoderBase
{
public:
	HardDecoder(std::shared_ptr<VideoReader> ptrVideoReader);
	~HardDecoder();

	//************************************
	// Method:    initModule
	// FullName:  HardDecoder::initModule
	// Access:    public 
	// Returns:   int32_t 0:success -1:failed
	// Qualifier:
	// Parameter: const DecoderInitedInfo & info�����
	// brief: ��ʼ�����������������빤���߳�
	//************************************
	int32_t initModule(const DecoderInitedInfo& info, DataHandlerInitedInfo& dataHandlerInfo)override;


	//************************************
	// Method:    uninitModule
	// FullName:  HardDecoder::uninitModule
	// Access:    public 
	// Returns:   int32_t
	// brief: ����ʼ�������������̣߳��ͷ���Դ
	// Qualifier:
	//************************************
	int32_t uninitModule()override;


	//************************************
	// Method:    addPCMBuffer
	// FullName:  HardDecoder::addPCMBuffer
	// Access:    public 
	// Returns:   int32_t
	// Qualifier:
	// brief: ���PCMBuffer��vector�У���������Ƶ���ݻ�����vector�е�PCMBuffer��
	// Parameter: std::shared_ptr<Buffer> ptrPCMBuffer
	//************************************
	int32_t addPCMBuffer(std::shared_ptr<Buffer> ptrPCMBuffer);

	//************************************
	// Method:    addPacketQueue
	// FullName:  HardDecoder::addPacketQueue
	// Access:    public 
	// Returns:   int32_t
	// Qualifier:
	// brief: ��ӽ�������Ƶ���ݶ��е�vector�У���������Ƶ���ݻ�����vector�еĶ�����
	// Parameter: std::shared_ptr<MyPacketQueue<VideoCallbackInfo>> ptrPacketQueue
	//************************************
	int32_t addPacketQueue(std::shared_ptr<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptrPacketQueue);

	int32_t seekTo(double_t seekTime)override;

	void registerFinishedCallback(DecoderFinishedCallback callback)override;

private:
	//************************************
	// Method:    decode
	// FullName:  HardDecoder::decode
	// Access:    private 
	// Returns:   void
	// brief: �����̣߳��Ӵ����������ȡ���������ݰ������͵��ö�Ӧ�Ľ��뺯��
	// Qualifier:
	//************************************
	void decode()override;
	//************************************
	// Method:    decodeVideo
	// FullName:  HardDecoder::decodeVideo
	// Access:    private 
	// Returns:   void
	// Qualifier:
	// brief: ��Ƶ�����뺯��
	// Parameter: PacketWaitDecoded & packet������İ����������ݺ�����
	//************************************
	void decodeVideo(std::shared_ptr<PacketWaitDecoded> packet)override;
	//************************************
	// Method:    decodeAudio
	// FullName:  HardDecoder::decodeAudio
	// Access:    private 
	// Returns:   void
	// Qualifier:
	// brief: ��Ƶ�����뺯��
	// Parameter: PacketWaitDecoded & packet ������İ����������ݺ�����
	//************************************
	void decodeAudio(std::shared_ptr<PacketWaitDecoded> packet)override;

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
	std::shared_ptr<VideoReader> m_ptrVideoReader{ nullptr };
	AVFormatContext* fileFormat{ nullptr };

	AVBufferRef* m_ptrHWDeviceCtx;
	enum AVPixelFormat m_hwPixFormat;
	AVCodecContext* videoCodecContext{ nullptr };
	AVCodecContext* audioCodecContext{ nullptr };
	SwsContext* swsContext{ nullptr };
	SwrContext* swrContext{ nullptr };
	int m_iVideoStreamIndex;
	int m_iAudioStreamIndex;
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

	//������İ�����
	std::shared_ptr<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>> m_ptrQueNeedDecodedPacket;

//һ�������������ж�������ߣ���Ӧ�������
	std::mutex m_PcmBufferAddMutex;
	std::mutex m_VideoQueueAddMutex;
	std::vector<std::shared_ptr<Buffer>> m_vecPCMBufferPtr;	//��������Ƶ���ݶ���
	std::vector<std::shared_ptr <MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>>> m_vecQueDecodedPacket;	//��������Ƶ���ݶ���
};

