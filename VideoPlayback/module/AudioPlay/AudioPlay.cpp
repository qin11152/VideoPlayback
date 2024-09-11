#include "AudioPlay.h"

AudioPlay::AudioPlay(QObject *parent)
	: QObject(parent)
{
	initModule();
}

AudioPlay::~AudioPlay()
{
	if (m_ptrAudioOutput)
	{
		delete m_ptrAudioOutput;
		m_ptrAudioOutput = nullptr;
	}
}

bool AudioPlay::initOutputParameter(const AudioInfo& audioInfo)
{
	// ������Ƶ��ʽ
	QAudioFormat format;
	format.setSampleRate(audioInfo.audioSampleRate);           // ������
	format.setChannelCount(audioInfo.audioChannels);             // ͨ��������������
	format.setSampleSize(av_get_bytes_per_sample(audioInfo.audioFormat) * 8);              // ÿ��������λ��
	format.setCodec("audio/pcm");          // �������
	format.setByteOrder(QAudioFormat::LittleEndian); // �ֽ���
	format.setSampleType(QAudioFormat::SignedInt);   // ��������

	QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
	if (!info.isFormatSupported(format)) 
	{
		return false;
	}
	m_ptrAudioOutput = new QAudioOutput(info, format);
	return true;
}

bool AudioPlay::startPlay()
{
	if (m_ptrAudioOutput)
	{
		m_ptr_AudioDevice = m_ptrAudioOutput->start();
	}
	return nullptr == m_ptr_AudioDevice ? false : true;
}

void AudioPlay::inputPcmData(const QByteArray& pcmData)
{
	if (m_ptr_AudioDevice)
	{
		m_ptr_AudioDevice->write(pcmData);
	}
}

void AudioPlay::initModule()
{
	//m_ptrAudioOutput = new QAudioOutput();
}
