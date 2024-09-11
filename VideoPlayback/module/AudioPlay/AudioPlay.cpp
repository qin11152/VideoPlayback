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
	// 定义音频格式
	QAudioFormat format;
	format.setSampleRate(audioInfo.audioSampleRate);           // 采样率
	format.setChannelCount(audioInfo.audioChannels);             // 通道数（立体声）
	format.setSampleSize(av_get_bytes_per_sample(audioInfo.audioFormat) * 8);              // 每个样本的位数
	format.setCodec("audio/pcm");          // 编解码器
	format.setByteOrder(QAudioFormat::LittleEndian); // 字节序
	format.setSampleType(QAudioFormat::SignedInt);   // 样本类型

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
