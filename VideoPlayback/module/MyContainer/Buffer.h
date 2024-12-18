#pragma once

#include "CommonDef.h"

#include <mutex>

class MY_EXPORT Buffer
{
public:
	Buffer();
	~Buffer();

	Buffer(const Buffer& l);
	Buffer& operator=(const Buffer& l) = delete;

	void initBuffer(uint32_t bufferSize);
	void unInitBuffer();

	void setStartPos(uint32_t startPos) { m_uiStartPos = startPos; }
	void setEndPos(uint32_t endPos) { m_uiEndPos = endPos; }

	void appendData(uint8_t* data, uint32_t size);

	bool getBuffer(uint8_t* buffer, uint32_t size);

	bool clearBuffer();

private:
	bool m_bInited{ false };
	std::mutex m_mutex;
	uint32_t m_uiStartPos{ 0 };
	uint32_t m_uiEndPos{ 0 };
	uint32_t m_uiBufferSize{ 0 };
	uint8_t* m_pBuffer{ nullptr };
};

