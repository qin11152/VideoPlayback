#pragma once

#include "CommonDef.h"

class SourceBase
{
public:
	SourceBase() = default;
	virtual ~SourceBase() = default;

	virtual int seek(const SeekParams& params) = 0;

	virtual int pause() = 0;
	
	virtual int resume() = 0;
};

