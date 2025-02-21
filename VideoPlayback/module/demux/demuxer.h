#pragma once

#include "CommonDef.h"
#include "module/MyContainer/MyQueue.h"

class demuxer
{
public:
	demuxer();
	~demuxer();

	int32_t initModule(const VideoReaderInitedInfo& info, DecoderInitedInfo& decoderInfo);

	int32_t uninitModule();

	int32_t pause();

	int32_t resume();

	int32_t seek(const SeekParams& params);

	double getFrameRate();

	void seekOperate();

	bool getFinishedState()const { return m_bReadFinished; };

	void demux();
private:
	bool m_bInitState{ false };
	bool m_bRunningState{ false };
	AVFormatContext* formatContext{ nullptr };
	AVIOContext* m_ptrIOContext{ nullptr };

	std::atomic<bool> m_bReadFinished{ false };
	std::mutex m_ReadFinishedMutex;
	std::condition_variable m_ReadFinishedCV;

	int m_iVideoStreamIndex{ -1 };
	int m_iAudioStreamIndex{ -1 };

	std::thread m_demuxerThread;

	bool m_bPauseState{ false };
	std::mutex m_PauseMutex;
	std::condition_variable m_PauseCV;	//暂停时的条件变量

	bool m_bSeekState{ false };
	std::atomic<double_t> m_dSeekTime{ 0 };

	std::shared_ptr < MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>> m_ptrQueNeedDecodedPacket;
};

