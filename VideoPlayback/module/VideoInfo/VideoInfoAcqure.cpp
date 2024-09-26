#include "VideoInfoAcqure.h"

std::mutex VideoInfoAcqure::m_mutex;
std::shared_ptr<VideoInfoAcqure> VideoInfoAcqure::m_ptrVideoAcquire = nullptr;

VideoInfoAcqure* VideoInfoAcqure::getInstance()
{
	//¼ÓËø
	if (!m_ptrVideoAcquire)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_ptrVideoAcquire)
		{
			m_ptrVideoAcquire = std::shared_ptr<VideoInfoAcqure>(new VideoInfoAcqure());
		}
	}
	return m_ptrVideoAcquire.get();
}

int32_t VideoInfoAcqure::getVideoInfo(const char* fileName, MediaInfo& mediaInfo)
{
	AVFormatContext* formatContext = avformat_alloc_context();
	if (!formatContext) 
	{
		return (int32_t)ErrorCode::AllocateContextError;
	}

	// Open the video file
	if (avformat_open_input(&formatContext, fileName, nullptr, nullptr) != 0)
	{
		avformat_free_context(formatContext);
		return (int32_t)ErrorCode::OpenInputError;
	}

	// Retrieve stream information
	if (avformat_find_stream_info(formatContext, nullptr) < 0) 
	{
		avformat_close_input(&formatContext);
		return (int32_t)ErrorCode::FindStreamInfoError;
	}

	// Find the first video stream
	for (unsigned int i = 0; i < formatContext->nb_streams; ++i) 
	{
		AVCodecParameters* codecParameters = formatContext->streams[i]->codecpar;
		if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			// Get video information
			if (codecParameters)
			{
				mediaInfo.width = codecParameters->width;
				mediaInfo.height = codecParameters->height;
				AVRational frameRate = formatContext->streams[i]->avg_frame_rate;
				mediaInfo.fps = av_q2d(frameRate);
				mediaInfo.duration = formatContext->duration / (double)AV_TIME_BASE;
			}
		}
		else if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			if (codecParameters)
			{
				mediaInfo.audioBitrate = codecParameters->bit_rate;
				mediaInfo.audioSampleRate = codecParameters->sample_rate;
				mediaInfo.audioChannels = codecParameters->ch_layout.nb_channels;
				mediaInfo.bitDepth = codecParameters->bits_per_coded_sample;
			}
		}
	}
	// Clean up
	avformat_close_input(&formatContext);
	return (int32_t)ErrorCode::NoError;
}
