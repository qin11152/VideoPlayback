#pragma once

#include "CommonDef.h"
#include "module/MyContainer/Buffer.h"
#include "module/MyContainer/MyQueue.h"

class VideoDecoderBase
{
public:
	VideoDecoderBase() = default;
	virtual ~VideoDecoderBase() = default;

	virtual int32_t initModule(const DecoderInitedInfo& info, DataHandlerInitedInfo& dataHandlerInfo) = 0;

	virtual int32_t uninitModule() = 0;

	virtual void decode() = 0;

	virtual void decodeVideo(std::shared_ptr<PacketWaitDecoded> packet) = 0;

	virtual void decodeAudio(std::shared_ptr<PacketWaitDecoded> packet) = 0;

	virtual int32_t addPCMBuffer(std::shared_ptr<Buffer> ptrPCMBuffer) = 0;

	virtual int32_t addPacketQueue(std::shared_ptr<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptrPacketQueue) = 0;

protected:
	std::thread m_DecoderThread;
};

