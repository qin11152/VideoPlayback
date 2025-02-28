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
	if (m_bSeekState)
	{
		return -1;
	}
	m_bSeekState = true;
	pause();
	
	clearAllQueueAndBuffer();

	if (m_ptrDemuxer)
	{
		int ret = m_ptrDemuxer->seek(params);
		if (0 == ret)
		{
			m_ptrVideoDecoder->seekTo(params.m_dSeekTime);
			m_ptrAudioAndVideoOutput->seekTo(params);
		}
		else
		{
			resumeAllQueueAndBuffer();
			resume();
			return -1;
		}
	}
	resumeAllQueueAndBuffer();
	resume();
	m_bSeekState = false;
	return 0;
}

int LocalFileSource::pause()
{
	if (m_ptrDemuxer)
	{
		m_ptrDemuxer->pause();
	}
	if (m_ptrVideoDecoder)
	{
		m_ptrVideoDecoder->pause();
	}
	if (m_ptrAudioAndVideoOutput)
	{
		m_ptrAudioAndVideoOutput->pause();
	}
	return 0;
}

int LocalFileSource::resume()
{
	if (m_ptrDemuxer)
	{
		m_ptrDemuxer->resume();
	}
	if (m_ptrVideoDecoder)
	{
		m_ptrVideoDecoder->resume();
	}
	if (m_ptrAudioAndVideoOutput)
	{
		m_ptrAudioAndVideoOutput->resume();
	}
	return 0;
}

void LocalFileSource::nextFrame()
{
	if (m_bNextFrameState)
	{
		return;
	}
	m_bNextFrameState = true;
	m_ptrAudioAndVideoOutput->pause();
	m_ptrAudioAndVideoOutput->nextFrame();
	m_ptrDemuxer->resume();
	m_ptrVideoDecoder->resume();
	m_bNextFrameState = false;
}

void LocalFileSource::previousFrame()
{
	if (m_bPreviousState)
	{
		return;
	}
	m_bPreviousState = true;
	pause();
	clearAllQueueAndBuffer();
	auto curPts = m_ptrAudioAndVideoOutput->getCurrentVideoDts();
	//计算上一帧时间戳
	auto prePts = curPts - 1.0 / m_ptrDemuxer->getFrameRate();
	qDebug() << "current pts" << curPts << "last frame pts" << prePts << ",dst frame pts" << prePts - 0.5;
	SeekParams params{ prePts - 0.5 , prePts  , -1 , -1 , SeekType::SeekAbsolute };
	m_ptrDemuxer->seek(params);
	m_ptrVideoDecoder->seekTo(params.m_dSeekTime);
	m_ptrDemuxer->resume();
	m_ptrVideoDecoder->resume();
	resumeAllQueueAndBuffer();
	m_ptrAudioAndVideoOutput->previousFrame(params);
	m_bPreviousState = false;
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
	for (auto iter : m_vecQueDecodedAudioPacket)
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
	for (auto iter : m_vecQueDecodedAudioPacket)
	{
		iter->resume();
	}
}
