#pragma once

#include "CommonDef.h"

class YuvDataHandler
{
protected:
	using YuvCallBack = std::function<void(std::shared_ptr<VideoCallbackInfo> videoInfo)>;
	YuvCallBack m_YuvCallback;

public:
	YuvDataHandler() = default;
	virtual ~YuvDataHandler() = default;

	virtual int32_t initModule(const DataHandlerInitedInfo& info) = 0;
	virtual int32_t uninitModule() = 0;

	virtual void setCallback(YuvCallBack callback) = 0;

	virtual void pause() = 0;
	virtual void resume() = 0;
};

