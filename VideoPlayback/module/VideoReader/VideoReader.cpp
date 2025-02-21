#include "VideoReader.h"

int custom_read(void* opaque, uint8_t* buf, int buf_size) 
{
	//qDebug() << "call read:" << buf_size;
	FILE* file = (FILE*)opaque;
	int bytes_read = fread(buf, 1, buf_size, file);
	//qDebug() << "real read:" << bytes_read;
	if (bytes_read == 0) {
		return AVERROR_EOF; // 返回 EOF
	}
	return bytes_read;
}

// 自定义seek函数
int64_t custom_seek(void* opaque, int64_t offset, int whence)
{
	qDebug() << "call seek,type:" << whence << ",byte:" << offset;
	FILE* file = (FILE*)opaque;  // 将 opaque 转换回文件指针

	switch (whence) {
	case SEEK_SET:
		fseek(file, offset, SEEK_SET);
		break;
	case SEEK_CUR:
		fseek(file, offset, SEEK_CUR);
		break;
	case SEEK_END:
		fseek(file, offset, SEEK_END);
		break;
	case AVSEEK_SIZE:
	{
		// 获取文件大小
		int64_t current_position = ftell(file); // 保存当前指针位置
		fseek(file, 0, SEEK_END);               // 移动到文件末尾
		int64_t size = ftell(file);            // 获取文件大小
		fseek(file, current_position, SEEK_SET); // 恢复原位置
		return size;                           // 返回文件大小
	}
	default:
		return -1;
	}

	return ftell(file); // 返回当前文件位置
}

VideoReader::VideoReader()
{

}

VideoReader::~VideoReader()
{
	uninitModule();
}

int32_t VideoReader::initModule(const VideoReaderInitedInfo& info, DecoderInitedInfo& decoderInfo)
{
	if (m_bInitState)
	{
		return -1;
	}

	FILE* file = fopen(info.m_strFileName.c_str(), "rb");
	if (!file) 
	{
		fprintf(stderr, "Failed to open file\n");
		return -1;
	}

	// 分配缓冲区
	unsigned char* buffer = (unsigned char*)av_malloc(4096);
	if (!buffer)
	{
		fprintf(stderr, "Failed to allocate buffer\n");
		return -1;
	}

	m_ptrIOContext = avio_alloc_context(
		buffer, 4096, 0, file, custom_read, nullptr, custom_seek);
	if (!m_ptrIOContext) 
	{
		fprintf(stderr, "Failed to create AVIOContext\n");
		return -1;
	}

	formatContext = avformat_alloc_context();
	formatContext->pb = m_ptrIOContext;

	int ret = avformat_open_input(&formatContext, nullptr, nullptr, nullptr);
	if ( ret!= 0)
	{
		//获取错误码
		char errbuf[1024];
		av_strerror(ret, errbuf, 1024);
		qDebug() << "Failed to open file;" << errbuf;
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
			if (!info.m_bAtom)
			{
				decoderInfo.videoCodec = const_cast<AVCodec*>(codec);
				decoderInfo.videoCodecParameters = codecParameters;
			}
			else
			{
				decoderInfo.atomVideoCodec = const_cast<AVCodec*>(codec);
				decoderInfo.atomVideoCodecParameters = codecParameters;
			}
			decoderInfo.iVideoIndex = videoStreamIndex;
		}
		else if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex == -1)
		{
			audioStreamIndex = i;
			if (!info.m_bAtom)
			{
				decoderInfo.audioCodec = const_cast<AVCodec*>(codec);
				decoderInfo.audioCodecParameters = codecParameters;
			}
			else
			{
				decoderInfo.vecAtomAudioCodec.push_back(const_cast<AVCodec*>(codec));
				decoderInfo.vecAtomAudioCodecParameters.push_back(std::make_pair(codecParameters, (int32_t)i));
			}
		}
	}
	decoderInfo.outAudioInfo = info.outAudioInfo;
	decoderInfo.outVideoInfo = info.outVideoInfo;
	decoderInfo.iAudioIndex = audioStreamIndex;
	decoderInfo.m_bAtom = info.m_bAtom;
	decoderInfo.m_eDeviceType = info.m_eDeviceType;
	if (!info.m_bAtom)
	{
		decoderInfo.formatContext = formatContext;
		decoderInfo.ptrPacketQueue = info.ptrPacketQueue;
	}
	else
	{
		if (videoStreamIndex != -1)
		{
			decoderInfo.atomVideoFormatContext = formatContext;
			decoderInfo.ptrAtomVideoPacketQueue = info.ptrPacketQueue;
		}
		else
		{
			decoderInfo.vecAudioFormatContext.push_back(formatContext);
			decoderInfo.vecAtomAudioPacketQueue.push_back(info.ptrPacketQueue);
		}
	}

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
	m_bPauseState = false;
	m_PauseCV.notify_all();
	m_ReadFinishedCV.notify_all();
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

int32_t VideoReader::pause()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_bPauseState = true;
	return 0;
}

int32_t VideoReader::resume()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_bPauseState = false;
	m_PauseCV.notify_one();
	m_bReadFinished = false;
	m_ReadFinishedCV.notify_one();
	return 0;
}

void VideoReader::readFrameFromFile()
{
	if (!m_bInitState)
	{
		return;
	}

	while (m_bRunningState)
	{
		AVPacket* packet = av_packet_alloc(); // 分配一个数据包
		if (!m_bRunningState)
		{
			break;
		}
		{
			std::unique_lock<std::mutex> lck(m_PauseMutex);
			if (m_bPauseState)
			{
				m_PauseCV.wait(lck, [this]() {return !m_bPauseState || !m_bRunningState; });
			}
		}
		if (av_read_frame(formatContext, packet) >= 0)
		{
			if (packet->stream_index == audioStreamIndex)
			{
				// 音频包需要解码
				if (m_ptrQueNeedDecodedPacket)
				{
					m_ptrQueNeedDecodedPacket->pushPacket(std::make_shared<PacketWaitDecoded>(packet, PacketType::Audio)); // 把音频包加入队列
				}
			}
			else if (packet->stream_index == videoStreamIndex)
			{
				// 视频包需要解码
				if (m_ptrQueNeedDecodedPacket)
				{
					m_ptrQueNeedDecodedPacket->pushPacket(std::make_shared<PacketWaitDecoded>(packet, PacketType::Video)); // 把视频包加入队列
				}
			}
			//std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		else
		{
			// 现在读取到文件末尾就退出
			av_packet_unref(packet);
			m_bReadFinished = true;
			std::unique_lock <std::mutex> lck(m_ReadFinishedMutex);
			//int ret = av_seek_frame(formatContext, videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
			m_ReadFinishedCV.wait(lck, [this] {return !m_bReadFinished || !m_bRunningState; });
		}
		//LOG_INFO("Read End");
	}
}
