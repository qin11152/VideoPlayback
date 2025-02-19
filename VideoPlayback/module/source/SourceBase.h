#pragma once
class SourceBase
{
public:
	SourceBase() = default;
	virtual ~SourceBase() = default;

	virtual int seek() = 0;

	virtual int pause() = 0;
};

