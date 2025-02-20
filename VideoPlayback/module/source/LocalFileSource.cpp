#include "LocalFileSource.h"

std::atomic<bool> LocalFileSource::m_bDemuxerFinished{ false };
std::atomic<bool> LocalFileSource::m_bDecoderFinished{ false };

LocalFileSource::LocalFileSource()
{

}

LocalFileSource::~LocalFileSource()
{

}

int LocalFileSource::seek(const SeekParams& params)
{
	pause();
	
	clearAllQueueAndBuffer();

	if (m_ptrDemuxer)
	{
		int ret = m_ptrDemuxer->seek(params);
		if (0 == ret)
		{
			m_ptrVideoDecoder->seekTo(params.m_dSeekTime);
			m_ptrPreviewAndPlay->seekTo(params);
		}
		else
		{
			resumeAllQueueAndBuffer();
			resume();
			return -1;
		}
	}
	resumeAllQueueAndBuffer();
	if (SeekType::SeekAbsolute == params.seekType)
	{
		resume();
	}
	else if (SeekType::SeekFrameStep == params.seekType)
	{
		//不恢复消耗，队列恢复
	}

	return 0;
}

int LocalFileSource::pause()
{
	if (m_ptrDemuxer)
	{
		m_ptrDemuxer->pause();
	}
	if (m_ptrPreviewAndPlay)
	{
		m_ptrPreviewAndPlay->pause();
	}
	return 0;
}

int LocalFileSource::resume()
{
	if (m_ptrDemuxer)
	{
		m_ptrDemuxer->resume();
	}
	if (m_ptrPreviewAndPlay)
	{
		m_ptrPreviewAndPlay->resume();
	}
	return 0;
}

void LocalFileSource::nextFrame()
{
	m_ptrPreviewAndPlay->nextFrame();
}

void LocalFileSource::setDemuxerFinishState(bool state)
{
	m_bDemuxerFinished = state;
}

bool LocalFileSource::getDemuxerFinishState()
{
	return m_bDemuxerFinished;
}

void LocalFileSource::setDecoderFinishState(bool state)
{
	m_bDecoderFinished = state;
}

bool LocalFileSource::getDecoderFinishState()
{
	return m_bDecoderFinished;
}

void LocalFileSource::clearAllQueueAndBuffer()
{
	m_ptrQueueWaitedDecodedPacket->clearQueue();
	for (auto iter : m_vecQueDecodedPacket)
	{
		iter->clearQueue();
	}
	for (auto iter : m_vecPCMBufferPtr)
	{
		iter->clearBuffer();
	}
}

void LocalFileSource::resumeAllQueueAndBuffer()
{
	m_ptrQueueWaitedDecodedPacket->resume();
	for (auto iter : m_vecQueDecodedPacket)
	{
		iter->resume();
	}
}
