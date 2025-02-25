#pragma once

#include "CommonDef.h"
#include "module/source/SourceBase.h"
#include "module/demux/demuxer.h"
#include "module/VideoDecoder/VideoDecoderBase.h"
#include "module/output/AudioAndVideoOutput.h"
#include "module/decoderedDataHandler/PreviewAndPlay/PreviewAndPlay.h"

class LocalFileSource :public SourceBase
{
public:
	LocalFileSource();
	~LocalFileSource();

	int seek(const SeekParams& params) override;
	int pause() override;
	int resume() override;

	void nextFrame();

	void previousFrame();

	static void setDemuxerFinishState(bool state);
	static bool getDemuxerFinishState();

	static void setDecoderFinishState(bool state);
	static bool getDecoderFinishState();

	std::shared_ptr<demuxer> m_ptrDemuxer{ nullptr };
	std::shared_ptr<VideoDecoderBase> m_ptrVideoDecoder{ nullptr };

	std::shared_ptr<AudioAndVideoOutput> m_ptrAudioAndVideoOutput{ nullptr };
	//std::shared_ptr<PreviewAndPlay> m_ptrPreviewAndPlay{ nullptr };

	//存放等待解码的avpacket
	std::shared_ptr < MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>> m_ptrQueueWaitedDecodedPacket{ nullptr };

	//存放解码后的视频和音频
	std::vector<std::shared_ptr<Buffer>> m_vecPCMBufferPtr;	
	std::vector<std::shared_ptr <MyPacketQueue<std::shared_ptr<DecodedImageInfo>>>> m_vecQueDecodedPacket;
	std::vector<std::shared_ptr <MyPacketQueue<std::shared_ptr<DecodedAudioInfo>>>> m_vecQueDecodedAudioPacket;

private:
	void clearAllQueueAndBuffer();
	void resumeAllQueueAndBuffer();

private:
	static std::atomic<bool> m_bDemuxerFinished;
	static std::atomic<bool> m_bDecoderFinished;
};

