#include "Buffer.h"

Buffer::Buffer()
{
}

Buffer::Buffer(const Buffer& l)
	: m_uiBufferSize(l.m_uiBufferSize), m_uiStartPos(l.m_uiStartPos), m_uiEndPos(l.m_uiEndPos)
{
	if (l.m_pBuffer)
	{
		m_pBuffer = new uint8_t[m_uiBufferSize];
		memcpy(m_pBuffer, l.m_pBuffer, m_uiBufferSize);
	}
	else
	{
		m_pBuffer = nullptr;
	}
}

Buffer::~Buffer()
{
	unInitBuffer();
}

void Buffer::initBuffer(uint32_t bufferSize)
{
	m_pBuffer = new uint8_t[bufferSize]{ 0 };
	m_uiBufferSize = bufferSize;
	m_bInited = true;
}

void Buffer::unInitBuffer()
{
	std::lock_guard<std::mutex> lck(m_mutex);
	if (m_pBuffer)
	{
		delete[] m_pBuffer;
		m_pBuffer = nullptr;
	}
	m_bInited = false;
}

void Buffer::appendData(uint8_t* data, uint32_t size)
{
	std::lock_guard<std::mutex> lck(m_mutex);
	QByteArray datas((char*)data, size);
	//如果datas的收个成员是0x0D，则打印hh
	if (m_uiEndPos + size > m_uiBufferSize)
	{
		memcpy(m_pBuffer, m_pBuffer + m_uiStartPos, m_uiEndPos - m_uiStartPos);
		m_uiEndPos -= m_uiStartPos;
		m_uiStartPos = 0;
	}
	if (m_uiEndPos + size > m_uiBufferSize)
	{
		//二倍扩容
		uint8_t* pTemp = new uint8_t[m_uiBufferSize * 2]{ 0 };
		memcpy(pTemp, m_pBuffer + m_uiStartPos, m_uiBufferSize);
		delete[] m_pBuffer;
		m_pBuffer = pTemp;
		m_uiBufferSize *= 2;
	}
	memcpy(m_pBuffer + m_uiEndPos, data, size);
	m_uiEndPos += size;
}

bool Buffer::getBuffer(uint8_t* buffer, uint32_t size)
{
	if (!m_bInited)
	{
		return false;
	}
	std::lock_guard<std::mutex> lck(m_mutex);
	if (m_uiStartPos + size > m_uiEndPos)
	{
		return false;
	}
	memcpy(buffer, m_pBuffer + m_uiStartPos, size);
	m_uiStartPos += size;
	return true;
}
