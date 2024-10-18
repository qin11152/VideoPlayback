#pragma once

#include "CommonDef.h"

#include <QObject>
#include <QAudioOutput>

class AudioPlay  : public QObject
{
	Q_OBJECT

public:
	AudioPlay(QObject *parent);
	~AudioPlay();

	bool initOutputParameter(const AudioInfo& audioInfo);

	bool startPlay();

	void inputPcmData(const QByteArray& pcmData);

	void clearAudioDevice();

private:
	void initModule();

private:
	QAudioOutput* m_ptrAudioOutput{ nullptr };
	QIODevice* m_ptr_AudioDevice{ nullptr };
};
