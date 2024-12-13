#pragma once
/*!
 * \file VideoReader.h
 * \date 2024/12/09 17:07
 *
 * \author DELL
 * Contact: user@company.com
 *
 * \brief ���ڴ���Ƶ�ļ��н�������Ƶ֡����Ƶ֡
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

	int32_t pause();

	int32_t resume();

	bool getFinishedState()const { return m_bReadFinished; };

private:
	void readFrameFromFile();

private:
	bool m_bInitState{ false };
	bool m_bRunningState{ false };
	AVFormatContext* formatContext{ nullptr };

	std::atomic<bool> m_bReadFinished{ false };
	std::mutex m_ReadFinishedMutex;
	std::condition_variable m_ReadFinishedCV;

	int videoStreamIndex{ -1 };
	int audioStreamIndex{ -1 };

	std::thread m_ReadThread;

	bool m_bPauseState{ false };
	std::mutex m_PauseMutex;
	std::condition_variable m_PauseCV;	//��ͣʱ����������

	std::shared_ptr < MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>> m_ptrQueNeedDecodedPacket;
};

