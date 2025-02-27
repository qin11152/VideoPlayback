#define _CRT_SECURE_NO_WARNINGS

#include "VideoDecoder.h"
#include "ui/VideoPlayback.h"
#include "module/utils/utils.h"
#include "module/source/LocalFileSource.h"

#define MAX_AUDIO_FRAME_SIZE 80960

VideoDecoder::VideoDecoder(std::shared_ptr<demuxer> ptrDemuxer)
	: videoCodecContext(nullptr), audioCodecContext(nullptr),
	m_iVideoStreamIndex(-1), m_iAudioStreamIndex(-1)
{

}

VideoDecoder::~VideoDecoder()
{
	uninitModule();
}

int32_t VideoDecoder::initModule(const DecoderInitedInfo& info, DataHandlerInitedInfo& dataHandlerInfo)
{
	if (m_bInitState)
	{
		return -1;
	}
	if (nullptr == info.formatContext)
	{
		return -1;
	}

	m_stuAudioInfo = info.outAudioInfo;
	m_stuVideoInfo = info.outVideoInfo;
	m_iVideoStreamIndex = info.iVideoIndex;
	m_iAudioStreamIndex = info.iAudioIndex;
	fileFormat = info.formatContext;

	if (0 != initVideoDecoder(info))
	{
		return -1;
	}

	if (0 != initAudioDecoder(info))
	{
		return -1;
	}

	AVRational frameRate = info.formatContext->streams[m_iVideoStreamIndex]->avg_frame_rate;
	auto fps = av_q2d(frameRate);
	m_uiReadThreadSleepTime = (kmilliSecondsPerSecond / fps);
	m_uiPerFrameSampleCnt = m_stuAudioInfo.audioSampleRate / fps;
	m_dFrameDuration = av_q2d(info.formatContext->streams[m_iVideoStreamIndex]->time_base);

	dataHandlerInfo.uiNeedSleepTime = m_uiReadThreadSleepTime;
	dataHandlerInfo.uiPerFrameSampleCnt = m_uiPerFrameSampleCnt;

	m_ptrQueNeedDecodedPacket = info.ptrPacketQueue;

	m_bInitState = true;
	m_bRunningState = true;

	m_DecoderThread = std::thread(&VideoDecoder::decode, this);
	return 0;
}

int32_t VideoDecoder::uninitModule()
{
	m_bRunningState = false;
	for (auto& it : m_vecPCMBufferPtr)
	{
		it->unInitBuffer();
	}
	for (auto& it : m_vecQueDecodedVideoPacket)
	{
		it->uninitModule();
	}
	if (m_ptrQueNeedDecodedPacket)
	{
		m_ptrQueNeedDecodedPacket->uninitModule();
	}
	if (m_DecoderThread.joinable())
	{
		m_DecoderThread.join();
	}
	m_vecQueDecodedVideoPacket.clear();
	m_vecPCMBufferPtr.clear();
	m_iAudioStreamIndex = -1;
	m_iVideoStreamIndex = -1;
	if (swsContext)
	{
		sws_freeContext(swsContext);
		swsContext = nullptr;
	}
	if (swrContext)
	{
		swr_free(&swrContext);
		swrContext = nullptr;
	}

	if (videoCodecContext)
	{
		avcodec_free_context(&videoCodecContext);
		videoCodecContext = nullptr;
	}
	if (audioCodecContext)
	{
		avcodec_free_context(&audioCodecContext);
		audioCodecContext = nullptr;
	}
	m_bInitState = false;
	return 0;
}

int32_t VideoDecoder::addPCMBuffer(std::shared_ptr<Buffer> ptrPCMBuffer)
{
	std::unique_lock<std::mutex> lck(m_PcmBufferAddMutex);
	if (std::find(m_vecPCMBufferPtr.begin(), m_vecPCMBufferPtr.end(), ptrPCMBuffer) == m_vecPCMBufferPtr.end())
	{
		m_vecPCMBufferPtr.push_back(ptrPCMBuffer);
	}
	else
	{
		return -1;
	}
	return 0;
}

int32_t VideoDecoder::addPacketQueue(std::shared_ptr<MyPacketQueue<std::shared_ptr<DecodedImageInfo>>> ptrPacketQueue)
{
	std::unique_lock<std::mutex> lck(m_VideoQueueAddMutex);
	if (std::find(m_vecQueDecodedVideoPacket.begin(), m_vecQueDecodedVideoPacket.end(), ptrPacketQueue) == m_vecQueDecodedVideoPacket.end())
	{
		m_vecQueDecodedVideoPacket.push_back(ptrPacketQueue);
	}
	else
	{
		return -1;
	}
	return 0;
}

int32_t VideoDecoder::addAudioPacketQueue(std::shared_ptr<MyPacketQueue<std::shared_ptr<DecodedAudioInfo>>> ptrPacketQueue)
{
	std::lock_guard<std::mutex> lck(m_AudioQueueAddMutex);
	if (std::find(m_vecQueueDecodedAudioPacket.begin(), m_vecQueueDecodedAudioPacket.end(), ptrPacketQueue) == m_vecQueueDecodedAudioPacket.end())
	{
		m_vecQueueDecodedAudioPacket.push_back(ptrPacketQueue);
	}
	else
	{
		return -1;
	}
	return 0;
}

void VideoDecoder::pause()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_bPauseState = true;
}

void VideoDecoder::resume()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_bPauseState = false;
	m_PauseCV.notify_one();
}

void VideoDecoder::decode()
{
	if (!m_bInitState)
	{
		return;
	}
	while (true)
	{
		if (!m_bRunningState)
		{
			break;
		}
		{
			std::unique_lock<std::mutex> lck(m_PauseMutex);
			if (m_bPauseState)
			{
				m_PauseCV.wait(lck, [this]() {return !m_bRunningState || !m_bPauseState; });
			}
		}
		if (LocalFileSource::getDemuxerFinishState() && 0 == m_ptrQueNeedDecodedPacket->getSize())
		{
			if (!m_bDecoderedFinished)
			{
				flushDecoder();
				m_bDecoderedFinished = true;
				if (m_finishedCallback)
				{
					m_finishedCallback();
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		std::shared_ptr<PacketWaitDecoded> packet = nullptr;
		if (m_ptrQueNeedDecodedPacket)
		{
			m_ptrQueNeedDecodedPacket->getPacket(packet);
		}
		if (packet)
		{
			LOG_INFO("Get One Packet");
			switch (packet->type)
			{
			case PacketType::Video:
			{
				auto start = std::chrono::steady_clock::now();
				decodeVideo(packet);
				auto end = std::chrono::steady_clock::now();
				//printf("decoder time:%lld\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
				break;
			}
			case PacketType::Audio:
			{
				decodeAudio(packet);
				break;
			}
			default:
				break;
			}
		}
		LOG_INFO("Decoder End");
	}
}

void VideoDecoder::decodeAudio(std::shared_ptr<PacketWaitDecoded> packet)
{
	AVFrame* frame = av_frame_alloc();
	int swr_size = 0;
	int resampled_linesize;
	int max_resampled_samples = 0;
	uint8_t** resampled_data = nullptr;
	if (avcodec_send_packet(audioCodecContext, packet->packet) == 0)
	{
		while (avcodec_receive_frame(audioCodecContext, frame) == 0)
		{
			LOG_INFO("Audio Decoder Begin Handle");
			//std::fstream fs("audio.pcm", std::ios::app | std::ios::binary);
			////���ز���֮ǰ�����ݱ��汾��
			//fs.write((const char *)frame->data[0], frame->linesize[0]);
			//fs.close();

			int resampled_samples = av_rescale_rnd(
				swr_get_delay(swrContext, audioCodecContext->sample_rate) + frame->nb_samples,
				m_stuAudioInfo.audioSampleRate, audioCodecContext->sample_rate, AV_ROUND_UP);

			if (resampled_samples > max_resampled_samples)
			{
				if (resampled_data)
				{
					av_freep(&resampled_data[0]);
				}
				av_samples_alloc_array_and_samples(&resampled_data, &resampled_linesize,
					kOutputAudioChannels, resampled_samples,
					m_stuAudioInfo.audioFormat, 0);
				max_resampled_samples = resampled_samples;
			}

			int converted_samples = swr_convert(swrContext, resampled_data, resampled_samples,
				(const uint8_t**)frame->data, frame->nb_samples);

			int pcmNumber = converted_samples * kOutputAudioChannels * av_get_bytes_per_sample(m_stuAudioInfo.audioFormat);

			//std::fstream fsbefore("audio_before_resample", std::ios::app | std::ios::binary);
			//fsbefore.write((const char*)frame->data[0], audioCodecContext->channels*frame->nb_samples* av_get_bytes_per_sample(audioCodecContext->sample_fmt));
			//fsbefore.close();

			//std::fstream fs("audio.pcm", std::ios::app | std::ios::binary);
			////���ز���֮������ݱ��汾��
			//fs.write((const char*)resampled_data[0], pcmNumber);
			//fs.close();

			if (resampled_data)
			{
				for (auto& it : m_vecPCMBufferPtr)
				{
					it->appendData(resampled_data[0], pcmNumber);
				}
			}
		}
	}
	LOG_INFO("Decoder Audio End");
	av_frame_free(&frame);
	if (resampled_data)
	{
		av_freep(&resampled_data[0]);
	}
}

int32_t VideoDecoder::initVideoDecoder(const DecoderInitedInfo& info)
{
	if (nullptr == info.videoCodecParameters)
	{
		return -1;
	}
	videoCodecContext = avcodec_alloc_context3(info.videoCodec);

	videoCodecContext->thread_count = 16; // ����ʵ�� CPU ����������
	videoCodecContext->thread_type = FF_THREAD_FRAME; // ��֡���н���

	videoCodecContext->flags |= AVFMT_FLAG_GENPTS;

	if (avcodec_parameters_to_context(videoCodecContext, info.videoCodecParameters) < 0)
	{
		avcodec_free_context(&videoCodecContext);
		return (int32_t)ErrorCode::AllocateContextError;
	}

	// �򿪽�����
	if (avcodec_open2(videoCodecContext, info.videoCodec, nullptr) < 0)
	{
		avcodec_free_context(&videoCodecContext);
		return (int32_t)ErrorCode::AllocateContextError;
	}

	swsContext = sws_getContext(
		videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
		m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat,
		SWS_BILINEAR, nullptr, nullptr, nullptr);
	return 0;
}

int32_t VideoDecoder::initAudioDecoder(const DecoderInitedInfo& info)
{
	if (nullptr == info.audioCodec || nullptr == info.audioCodecParameters)
	{
		return -1;
	}
	audioCodecContext = avcodec_alloc_context3(info.audioCodec);
	avcodec_parameters_to_context(audioCodecContext, info.audioCodecParameters);
	avcodec_open2(audioCodecContext, info.audioCodec, nullptr);

	swrContext = swr_alloc();
	if (!swrContext)
	{
		return (int32_t)ErrorCode::AllocateRsampleError;
	}

	AVChannelLayout in_channel_layout;
	av_channel_layout_default(&in_channel_layout, audioCodecContext->ch_layout.nb_channels);

	AVChannelLayout out_channel_layout;
	av_channel_layout_default(&out_channel_layout, m_stuAudioInfo.audioChannels); // Stereo output

	int out_sample_rate = m_stuAudioInfo.audioSampleRate;
	// int out_sample_rate = audioCodecContext->sample_rate;
	AVSampleFormat out_sample_fmt = m_stuAudioInfo.audioFormat;

	if (swr_alloc_set_opts2(&swrContext, &out_channel_layout, out_sample_fmt, out_sample_rate,
		&in_channel_layout, audioCodecContext->sample_fmt, audioCodecContext->sample_rate, 0, nullptr) < 0)
	{
		swr_free(&swrContext);
		return (int32_t)ErrorCode::AllocateRsampleError;
	}

	if (swr_init(swrContext) < 0)
	{
		swr_free(&swrContext);
		return (int32_t)ErrorCode::AllocateRsampleError;
	}
	return 0;
}

void VideoDecoder::flushDecoder()
{
	AVFrame* frame = av_frame_alloc();
	AVFrame* swFrame = nullptr;
	avcodec_send_packet(videoCodecContext, NULL);

	// ������ȡ���л������е�֡
	while (avcodec_receive_frame(videoCodecContext, frame) >= 0)
	{
		double pts = frame->pts * m_dFrameDuration;
		std::shared_ptr<DecodedImageInfo> videoInfo = std::make_shared<DecodedImageInfo>();
		videoInfo->width = videoCodecContext->width;
		videoInfo->height = videoCodecContext->height;
		videoInfo->videoFormat = videoCodecContext->pix_fmt;
		videoInfo->m_dPts = pts;
		// �����Ӳ��֡����Ҫת����ϵͳ�ڴ�
		switch (videoCodecContext->pix_fmt)
		{
		default:
		{
			AVFrame* yuvFrame = av_frame_alloc();
			av_image_alloc(yuvFrame->data, yuvFrame->linesize, m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat, 1);
			if (swsContext)
			{
				sws_scale(swsContext, frame->data, frame->linesize, 0, videoCodecContext->height, yuvFrame->data, yuvFrame->linesize);
				LOG_INFO("Video Decoder Convert");
			}
			videoInfo->videoFormat = m_stuVideoInfo.videoFormat;
			videoInfo->width = m_stuVideoInfo.width;
			videoInfo->height = m_stuVideoInfo.height;
			//int bufferSize = av_image_get_buffer_size(videoCodecContext->pix_fmt, videoCodecContext->width, videoCodecContext->height, 1);
			videoInfo->dataSize = m_stuVideoInfo.width * m_stuVideoInfo.height * 2;
			videoInfo->yuvData = new uint8_t[m_stuVideoInfo.width * m_stuVideoInfo.height * 2];
			memcpy(videoInfo->yuvData, yuvFrame->data[0], m_stuVideoInfo.width * m_stuVideoInfo.height * 2);
			av_freep(yuvFrame->data);
			av_frame_free(&yuvFrame);
		}
		break;
		}
		for (auto& it : m_vecQueDecodedVideoPacket)
		{
			it->pushPacket(videoInfo);
		}
		// ����frame
	}
}

void VideoDecoder::decodeVideo(std::shared_ptr<PacketWaitDecoded> packet)
{
	AVFrame* frame = av_frame_alloc();
	if (!packet)
	{
		return;
	}
	if (avcodec_send_packet(videoCodecContext, packet->packet) == 0)
	{
		int ret = avcodec_receive_frame(videoCodecContext, frame);
		while (ret == 0)
		{
			LOG_INFO("Video Decoder Begin Handle");
			double pts = frame->pts * av_q2d(fileFormat->streams[m_iVideoStreamIndex]->time_base);

			std::shared_ptr<DecodedImageInfo> videoInfo = std::make_shared<DecodedImageInfo>();
			videoInfo->width = videoCodecContext->width;
			videoInfo->height = videoCodecContext->height;
			videoInfo->videoFormat = videoCodecContext->pix_fmt;
			videoInfo->m_dPts = pts;
			// ����avframe�е�������
			switch (videoCodecContext->pix_fmt)
			{
			//case AV_PIX_FMT_YUV420P:
			//{
			//	videoInfo->yuvData = new uint8_t[videoCodecContext->width * videoCodecContext->height * 3 / 2];
			//	videoInfo->dataSize = videoCodecContext->width * videoCodecContext->height * 3 / 2;
			//	memcpy(videoInfo->yuvData, frame->data[0], videoCodecContext->width * videoCodecContext->height);
			//	memcpy(videoInfo->yuvData + videoCodecContext->width * videoCodecContext->height,
			//		frame->data[1],
			//		videoCodecContext->width / 2 * videoCodecContext->height / 2);

			//	// 4. ���� V ����
			//	memcpy(videoInfo->yuvData + videoCodecContext->width * videoCodecContext->height +
			//		videoCodecContext->width / 2 * videoCodecContext->height / 2,
			//		frame->data[2],
			//		videoCodecContext->width / 2 * videoCodecContext->height / 2);
			//}
			//break;
			//case AV_PIX_FMT_YUV422P:
			//{
			//	videoInfo->yuvData = new uint8_t[videoCodecContext->width * videoCodecContext->height * 2];
			//	videoInfo->dataSize = videoCodecContext->width * videoCodecContext->height * 2;
			//	int ySize = videoCodecContext->width * videoCodecContext->height;
			//	int uSize = videoCodecContext->width * videoCodecContext->height / 2; // U �� V �Ĵ�С�� Y ��һ��
			//	memcpy(videoInfo->yuvData, frame->data[0], ySize);
			//	memcpy(videoInfo->yuvData + ySize, frame->data[1], uSize);
			//	memcpy(videoInfo->yuvData + ySize + uSize, frame->data[2], uSize);
			//}
			//break;
			//case AV_PIX_FMT_YUYV422:
			//{
			//	videoInfo->yuvData = new uint8_t[videoCodecContext->width * videoCodecContext->height * 2];
			//	videoInfo->dataSize = videoCodecContext->width * videoCodecContext->height * 2;
			//	int size = videoCodecContext->width * videoCodecContext->height * 2; // ÿ������ 2 �ֽ�

			//	// ���� YUYV ����
			//	for (int y = 0; y < videoCodecContext->height; ++y)
			//	{
			//		// ����Դ���ݵ���ʼλ��
			//		uint8_t* src = frame->data[0] + y * frame->linesize[0];
			//		// ����Ŀ�����ݵ���ʼλ��
			//		uint8_t* dst = videoInfo->yuvData + y * videoCodecContext->width * 2;

			//		// ����ÿһ�е�����
			//		for (int x = 0; x < videoCodecContext->width; x += 2) {
			//			// ���� Y0 �� U
			//			dst[0] = src[2 * x];     // Y0
			//			dst[1] = src[2 * x + 1]; // U
			//			dst[2] = src[2 * x + 2]; // Y1
			//			dst[3] = src[2 * x + 3]; // V
			//			dst += 4; // �ƶ�����һ������
			//		}
			//	}
			//}
			//break;
			//case AV_PIX_FMT_UYVY422:
			//{
			//	videoInfo->yuvData = new uint8_t[videoCodecContext->width * videoCodecContext->height * 2];
			//	videoInfo->dataSize = videoCodecContext->width * videoCodecContext->height * 2;
			//	int size = videoCodecContext->width * videoCodecContext->height * 2; // ÿ������ 2 �ֽ�

			//	// ���� UYVY ����
			//	for (int y = 0; y < videoCodecContext->height; ++y) {
			//		// ����Դ���ݵ���ʼλ��
			//		uint8_t* src = frame->data[0] + y * frame->linesize[0];
			//		// ����Ŀ�����ݵ���ʼλ��
			//		uint8_t* dst = videoInfo->yuvData + y * videoCodecContext->width * 2;

			//		for (int x = 0; x < videoCodecContext->width; x += 2) {
			//			// ���� U �� Y
			//			dst[0] = src[2 * x];     // U
			//			dst[1] = src[2 * x + 1]; // Y
			//			dst[2] = src[2 * x + 2]; // V
			//			dst[3] = src[2 * x + 1]; // Y
			//			dst += 4; // �ƶ�����һ������
			//		}
			//	}
			//}
			//break;
			default:
			{
				AVFrame* yuvFrame = av_frame_alloc();
				av_image_alloc(yuvFrame->data, yuvFrame->linesize, m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat, 1);
				if (swsContext)
				{
					sws_scale(swsContext, frame->data, frame->linesize, 0, videoCodecContext->height, yuvFrame->data, yuvFrame->linesize);
					LOG_INFO("Video Decoder Convert");
				}
				videoInfo->videoFormat = m_stuVideoInfo.videoFormat;
				videoInfo->width = m_stuVideoInfo.width;
				videoInfo->height = m_stuVideoInfo.height;
				//int bufferSize = av_image_get_buffer_size(videoCodecContext->pix_fmt, videoCodecContext->width, videoCodecContext->height, 1);
				videoInfo->dataSize = m_stuVideoInfo.width * m_stuVideoInfo.height * 2;
				videoInfo->yuvData = new uint8_t[m_stuVideoInfo.width * m_stuVideoInfo.height * 2];
				memcpy(videoInfo->yuvData, yuvFrame->data[0], m_stuVideoInfo.width * m_stuVideoInfo.height * 2);
				av_freep(yuvFrame->data);
				av_frame_free(&yuvFrame);
			}
			break;
			}
			for (auto& it : m_vecQueDecodedVideoPacket)
			{
				it->pushPacket(videoInfo);
			}
			ret = avcodec_receive_frame(videoCodecContext, frame);
		}
	}
	else
	{
		LOG_ERROR("video avcodec_send_packet error");
	}
	LOG_INFO("Decoder Video End");
	av_frame_free(&frame);
}

int32_t VideoDecoder::seekTo(double_t time)
{
	m_bDecoderedFinished = false;
	LocalFileSource::setDecoderFinishState(false);
	avcodec_flush_buffers(videoCodecContext);
	avcodec_flush_buffers(audioCodecContext);
	return 0;
}

void VideoDecoder::registerFinishedCallback(DecoderFinishedCallback callback)
{
	return;
}
