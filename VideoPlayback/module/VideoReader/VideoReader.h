#pragma once
/*!
 * \file VideoReader.h
 * \date 2024/12/09 17:07
 *
 * \author DELL
 * Contact: user@company.com
 *
 * \brief 用于从视频文件中解析出视频帧和音频帧
 *
 * TODO: long description
 *
 * \note
*/

#include "CommonDef.h"
#include "module/MyContainer/MyQueue.h"
#include "module/VideoDecoder/VideoDecoderBase.h"
#include "module/decoderedDataHandler/PreviewAndPlay/PreviewAndPlay.h"

class VideoReader
{
public:
	VideoReader();
	~VideoReader();

	int32_t initModule(const VideoReaderInitedInfo& info, DecoderInitedInfo& decoderInfo);

	int32_t uninitModule();

private:
	void readFrameFromFile();

private:
	bool m_bInitState{ false };
	bool m_bRunningState{ false };
	AVFormatContext* formatContext{ nullptr };

	int videoStreamIndex{ -1 };
	int audioStreamIndex{ -1 };

	std::thread m_ReadThread;
	std::condition_variable m_ReadCV;	//从文件中读的条件变量

	std::shared_ptr < MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>> m_ptrQueNeedDecodedPacket;
};

