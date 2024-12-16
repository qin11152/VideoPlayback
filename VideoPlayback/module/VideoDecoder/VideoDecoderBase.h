#pragma once

#include "CommonDef.h"
#include "module/MyContainer/Buffer.h"
#include "module/MyContainer/MyQueue.h"

class VideoDecoderBase
{

protected:
	using DecoderFinishedCallback = std::function<void()>;

	std::thread m_DecoderThread;
	DecoderFinishedCallback m_finishedCallback;

public:
	VideoDecoderBase() = default;
	virtual ~VideoDecoderBase() = default;

	virtual int32_t initModule(const DecoderInitedInfo& info, DataHandlerInitedInfo& dataHandlerInfo) = 0;

	virtual int32_t uninitModule() = 0;

	virtual void decode() {};

	virtual void decodeVideo(std::shared_ptr<PacketWaitDecoded> packet) {};

	virtual void decodeAudio(std::shared_ptr<PacketWaitDecoded> packet) {};

	virtual void decodeAudio(std::shared_ptr<PacketWaitDecoded> packet, int index) {};

	//************************************
	// Method:    addPCMBuffer
	// FullName:  VideoDecoderBase::addPCMBuffer
	// Access:    virtual public 
	// Returns:   int32_t
	// Qualifier:
	// brief: ���һ����Ƶbuffer������������������Ƶ���ݻ����ڴ�buffer��
	// Parameter: std::shared_ptr<Buffer> ptrPCMBuffer
	//************************************
	virtual int32_t addPCMBuffer(std::shared_ptr<Buffer> ptrPCMBuffer) { return 0; };

	//************************************
	// Method:    addPacketQueue
	// FullName:  VideoDecoderBase::addPacketQueue
	// Access:    virtual public 
	// Returns:   int32_t
	// Qualifier:
	// brief: ���һ�����е��������У����������ݷ��ڴ˶��У��ö��п��ж��������������ݿɹ����handlerʹ��
	// Parameter: std::shared_ptr<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptrPacketQueue
	//************************************
	virtual int32_t addPacketQueue(std::shared_ptr<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptrPacketQueue) { return 0; };

	//************************************
	// Method:    addAtomVideoPacketQueue
	// FullName:  VideoDecoderBase::addAtomVideoPacketQueue
	// Access:    virtual public 
	// Returns:   int32_t
	// brief: ���һ�����е��������У����������ݷ��ڴ˶��У��ö��п��ж��������������ݿɹ����handlerʹ��
	// Qualifier:
	// Parameter: std::shared_ptr<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptrPacketQueue
	//************************************
	virtual int32_t addAtomVideoPacketQueue(std::shared_ptr<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptrPacketQueue) { return 0; };

	//************************************
	// Method:    addAtomAudioPacketQueue
	// FullName:  VideoDecoderBase::addAtomAudioPacketQueue
	// Access:    virtual public 
	// Returns:   int32_t
	// Qualifier:
	// brief: ���һ����Ƶ����Buffer��vector���������У���Ϊatom���ܰ��������Ƶ�ļ���ÿ����Ƶ�ļ���Ӧһ��buffer�������ж��handler��ÿ��handler��Ӧһ��vector
	// Parameter: std::vector<std::shared_ptr<Buffer>>
	//************************************
	virtual int32_t addAtomAudioPacketQueue(std::shared_ptr<std::vector<std::shared_ptr<Buffer>>>) { return 0; };

	virtual int32_t seekTo(double_t seekTime) = 0;

	virtual void registerFinishedCallback(DecoderFinishedCallback callback) = 0;
};

