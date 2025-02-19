#include "LocalFileSource.h"

std::atomic<bool> LocalFileSource::m_bDemuxerFinished{ false };
std::atomic<bool> LocalFileSource::m_bDecoderFinished{ false };

LocalFileSource::LocalFileSource()
{

}

LocalFileSource::~LocalFileSource()
{

}

int LocalFileSource::seek()
{
	return 0;
}

int LocalFileSource::pause()
{
	return 0;
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
