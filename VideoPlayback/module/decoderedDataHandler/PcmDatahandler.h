#pragma once

#include "CommonDef.h"

class PcmDatahandler
{
protected:
	using AudioPlayCallback = std::function< void(std::shared_ptr<AudioCallbackInfo> audioInfo)>;

	AudioPlayCallback m_AudioCallback;

public:
	PcmDatahandler() = default;
	virtual ~PcmDatahandler() = default;

	virtual int32_t initModule(const DataHandlerInitedInfo& info) = 0;
	virtual int32_t uninitModule() = 0;

	virtual void setCallback(AudioPlayCallback callback) = 0;

	virtual void pause() = 0;
	virtual void resume() = 0;
};

