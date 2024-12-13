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
	// brief: 添加一个音频buffer到解码器，解码后的音频数据会存放在此buffer中
	// Parameter: std::shared_ptr<Buffer> ptrPCMBuffer
	//************************************
	virtual int32_t addPCMBuffer(std::shared_ptr<Buffer> ptrPCMBuffer) { return 0; };

	//************************************
	// Method:    addPacketQueue
	// FullName:  VideoDecoderBase::addPacketQueue
	// Access:    virtual public 
	// Returns:   int32_t
	// Qualifier:
	// brief: 添加一个队列到解码器中，解码后的数据放在此队列，该队列可有多个，解码出的数据可供多个handler使用
	// Parameter: std::shared_ptr<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptrPacketQueue
	//************************************
	virtual int32_t addPacketQueue(std::shared_ptr<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptrPacketQueue) { return 0; };

	//************************************
	// Method:    addAtomVideoPacketQueue
	// FullName:  VideoDecoderBase::addAtomVideoPacketQueue
	// Access:    virtual public 
	// Returns:   int32_t
	// brief: 添加一个队列到解码器中，解码后的数据放在此队列，该队列可有多个，解码出的数据可供多个handler使用
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
	// brief: 添加一个音频数据Buffer的vector到解码器中，因为atom可能包含多个音频文件，每个音频文件对应一个buffer，可以有多个handler，每个handler对应一个vector
	// Parameter: std::vector<std::shared_ptr<Buffer>>
	//************************************
	virtual int32_t addAtomAudioPacketQueue(std::shared_ptr<std::vector<std::shared_ptr<Buffer>>>) { return 0; };

	virtual int32_t seekTo(double_t seekTime) = 0;

	virtual void registerFinishedCallback(DecoderFinishedCallback callback) = 0;
};

