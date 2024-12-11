#include "VideoReader.h"

VideoReader::VideoReader()
{

}

VideoReader::~VideoReader()
{

}

int32_t VideoReader::initModule(const VideoReaderInitedInfo& info, DecoderInitedInfo& decoderInfo)
{
	if (m_bInitState)
	{
		return -1;
	}

	if (avformat_open_input(&formatContext, info.m_strFileName.c_str(), nullptr, nullptr) != 0)
	{
		return (int32_t)ErrorCode::OpenInputError;
	}

	if (avformat_find_stream_info(formatContext, nullptr) < 0)
	{
		avformat_close_input(&formatContext);
		return (int32_t)ErrorCode::FindStreamInfoError;
	}
	videoStreamIndex = -1;
	audioStreamIndex = -1;
	for (unsigned int i = 0; i < formatContext->nb_streams; ++i)
	{
		AVCodecParameters* codecParameters = formatContext->streams[i]->codecpar;
		const AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
		if (!codec)
		{
			continue;
		}

		if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex == -1)
		{
			videoStreamIndex = i;
			decoderInfo.videoCodec = const_cast<AVCodec*>(codec);
			decoderInfo.videoCodecParameters = codecParameters;
		}
		else if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex == -1)
		{
			audioStreamIndex = i;
			decoderInfo.audioCodec = const_cast<AVCodec*>(codec);
			decoderInfo.audioCodecParameters = codecParameters;
		}
	}
	decoderInfo.outAudioInfo = info.outAudioInfo;
	decoderInfo.outVideoInfo = info.outVideoInfo;
	decoderInfo.formatContext = formatContext;
	decoderInfo.iVideoIndex = videoStreamIndex;
	decoderInfo.iAudioIndex = audioStreamIndex;
	decoderInfo.m_bAtom = info.m_bAtom;
	decoderInfo.m_eDeviceType = info.m_eDeviceType;
	decoderInfo.ptrPacketQueue = info.ptrPacketQueue;

	m_bRunningState = true;
	m_ReadThread = std::thread(std::bind(&VideoReader::readFrameFromFile, this));
	m_ptrQueNeedDecodedPacket = info.ptrPacketQueue;
	m_bInitState = true;
	return (int32_t)ErrorCode::NoError;
}

int32_t VideoReader::uninitModule()
{
	if (!m_bInitState)
	{
		return -1;
	}
	m_bInitState = false;
	m_bRunningState = false;
	if (m_ptrQueNeedDecodedPacket)
	{
		m_ptrQueNeedDecodedPacket->uninitModule();
	}
	if (m_ReadThread.joinable())
	{
		m_ReadThread.join();
	}
	m_ptrQueNeedDecodedPacket = nullptr;
	return (int32_t)ErrorCode::NoError;
}

void VideoReader::readFrameFromFile()
{
	if (!m_bInitState)
	{
		return;
	}

	while (m_bRunningState)
	{
		AVPacket* packet = av_packet_alloc(); // ����һ�����ݰ�
		if (!m_bRunningState)
		{
			break;
		}
		if (av_read_frame(formatContext, packet) >= 0)
		{
			if (packet->stream_index == audioStreamIndex)
			{
				// ��Ƶ����Ҫ����
				if (m_ptrQueNeedDecodedPacket)
				{
					m_ptrQueNeedDecodedPacket->addPacket(std::make_shared<PacketWaitDecoded>(packet, PacketType::Audio)); // ����Ƶ���������
				}
			}
			else if (packet->stream_index == videoStreamIndex)
			{
				// ��Ƶ����Ҫ����
				if (m_ptrQueNeedDecodedPacket)
				{
					m_ptrQueNeedDecodedPacket->addPacket(std::make_shared<PacketWaitDecoded>(packet, PacketType::Video)); // ����Ƶ���������
				}
			}
			//std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		else
		{
			// ���ڶ�ȡ���ļ�ĩβ���˳�
			av_packet_unref(packet);
			break;
			// todo�������Ҫѭ�����ţ�����������seek���ļ���ͷ
		}
		LOG_INFO("Read End");
	}
}
