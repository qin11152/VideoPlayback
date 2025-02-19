#pragma once

#include "CommonDef.h"
#include "module/source/SourceBase.h"
#include "module/demux/demuxer.h"
#include "module/VideoDecoder/VideoDecoderBase.h"

class LocalFileSource :public SourceBase
{
public:
	LocalFileSource();
	~LocalFileSource();

	int seek() override;
	int pause() override;

	static void setDemuxerFinishState(bool state);
	static bool getDemuxerFinishState();

	static void setDecoderFinishState(bool state);
	static bool getDecoderFinishState();

	std::shared_ptr<demuxer> m_ptrDemuxer{ nullptr };
	std::shared_ptr<VideoDecoderBase> m_ptrVideoDecoder{ nullptr };

	//存放等待解码的avpacket
	std::shared_ptr < MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>> m_ptrQueueWaitedDecodedPacket{ nullptr };

	//存放解码后的视频
	std::shared_ptr<Buffer>m_ptrAudioBuffer{ nullptr };
	std::shared_ptr < MyPacketQueue<std::shared_ptr<DecodedImageInfo>>>m_ptrQueueDecodedImage{ nullptr };

private:
	static std::atomic<bool> m_bDemuxerFinished;
	static std::atomic<bool> m_bDecoderFinished;
};

