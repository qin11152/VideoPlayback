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

}

void Buffer::initBuffer(uint32_t bufferSize)
{
	m_pBuffer = new uint8_t[bufferSize]{ 0 };
	m_uiBufferSize = bufferSize;
}

void Buffer::unInitBuffer()
{
	if (m_pBuffer)
	{
		delete[] m_pBuffer;
		m_pBuffer = nullptr;
	}
}

void Buffer::appendData(uint8_t* data, uint32_t size)
{
	std::lock_guard<std::mutex> lck(m_mutex);
	if (m_uiEndPos + size > m_uiBufferSize)
	{
		memcpy(m_pBuffer, m_pBuffer + m_uiStartPos, m_uiEndPos - m_uiStartPos);
		m_uiEndPos -= m_uiStartPos;
		m_uiStartPos = 0;
	}
	if (m_uiEndPos + size > m_uiBufferSize)
	{
		//¶þ±¶À©ÈÝ
		uint8_t* pTemp = new uint8_t[m_uiBufferSize * 2]{ 0 };
		memcpy(pTemp, m_pBuffer, m_uiBufferSize);
		delete[] m_pBuffer;
		m_pBuffer = pTemp;
		m_uiBufferSize *= 2;
	}
	memcpy(m_pBuffer + m_uiEndPos, data, size);
	m_uiEndPos += size;
}

void Buffer::getBuffer(uint8_t* buffer, uint32_t size)
{
	std::lock_guard<std::mutex> lck(m_mutex);
	memcpy(buffer, m_pBuffer + m_uiStartPos, size);
	m_uiStartPos += size;
}
