#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "CommonDef.h"

#include <mutex>
#include <memory>

class VideoInfoAcqure
{
public:
	~VideoInfoAcqure() = default;

	static VideoInfoAcqure* getInstance();

	int32_t getVideoInfo(const char* fileName, MediaInfo& mediaInfo);
private:
	VideoInfoAcqure() = default;
	static std::shared_ptr<VideoInfoAcqure> m_ptrVideoAcquire;
	static std::mutex m_mutex;
};

