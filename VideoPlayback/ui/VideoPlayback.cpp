#include "VideoPlayback.h"
#include "ui/MySlider/MySlider.h"
#include "module/VideoInfo/VideoInfoAcqure.h"

#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QFileDialog>

IDeckLinkMutableVideoFrame *kDecklinkOutputFrame = nullptr;

VideoPlayback::VideoPlayback(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);

	initModule();

	connect(ui.startPushButton, &QPushButton::clicked, this, [=]()
			{
		if (m_ptrVideoDecoder)
		{
			delete m_ptrVideoDecoder;
			m_ptrVideoDecoder = nullptr;
		}
		m_ptrVideoDecoder = new VideoDecoder();
		if (initDecoder())
		{
			m_ptrVideoDecoder->startDecoder();
		} });
	connect(ui.pausePushButton, &QPushButton::clicked, this, [=]()
			{ m_ptrVideoDecoder->pauseDecoder(); });
	connect(ui.continuePushButton, &QPushButton::clicked, this, [=]()
			{ m_ptrVideoDecoder->resumeDecoder(); });
}

VideoPlayback::~VideoPlayback()
{
	if (m_ptrVideoDecoder)
	{
		delete m_ptrVideoDecoder;
		m_ptrVideoDecoder = nullptr;
	}

	if (m_ptrAudioPlay)
	{
		delete m_ptrAudioPlay;
		m_ptrAudioPlay = nullptr;
	}
}

bool VideoPlayback::initModule()
{
	m_ptrVideoDecoder = new VideoDecoder();
	m_ptrAudioPlay = new AudioPlay(nullptr);
#if defined(WIN32)
	m_ptrDeckLinkDeviceDiscovery = new DeckLinkDeviceDiscovery();
#elif defined(__linux__)
	m_ptrDeckLinkDeviceDiscovery = new DeckLinkDeviceDiscovery(this);
#endif
	m_ptrDeckLinkDeviceDiscovery->Enable();
	return initConnect() && initAudioOutput();
}

void VideoPlayback::previewCallback(const VideoCallbackInfo &videoInfo, int64_t currentTime)
{
	// 将YUV数据发送出去
	if (nullptr == videoInfo.yuvData)
	{
		return;
	}
	VideoInfo video;
	video.width = videoInfo.width;
	video.height = videoInfo.height;
	video.videoFormat = videoInfo.videoFormat;
	QByteArray data((char *)videoInfo.yuvData, videoInfo.dataSize);
	emit signalYUVData(data, video);
	updateTimeLabel(currentTime, m_stuMediaInfo.duration);
	updateTimeSliderPosition(currentTime);
}

void VideoPlayback::SDIOutputCallback(const VideoCallbackInfo &videoInfo)
{
	if (nullptr == videoInfo.yuvData || nullptr == m_ptrSelectedDeckLinkOutput)
	{
		return;
	}
	uint8_t *destData = nullptr;
	kDecklinkOutputFrame->GetBytes((void **)&destData);
	std::copy(videoInfo.yuvData, videoInfo.yuvData + videoInfo.dataSize, destData);
	int ret = m_ptrSelectedDeckLinkOutput->DisplayVideoFrameSync(kDecklinkOutputFrame);
	if (ret != S_OK)
	{
		LOG_ERROR("DisplayVideoFrameSync error");
	}
}

void VideoPlayback::AudioPlayCallBack(uint8_t **audioData, uint32_t channelSampleNumber)
{
	QByteArray data((char *)audioData[0], kOutputAudioChannels * channelSampleNumber * av_get_bytes_per_sample((AVSampleFormat)kOutputAudioFormat));
	m_ptrAudioPlay->inputPcmData(data);
	if (nullptr != m_ptrSelectedDeckLinkOutput)
	{
		uint8_t *destData = nullptr;
		destData = new uint8_t[kOutputAudioChannels * channelSampleNumber * av_get_bytes_per_sample((AVSampleFormat)kOutputAudioFormat)];
		memcpy(destData, audioData[0], kOutputAudioChannels * channelSampleNumber * av_get_bytes_per_sample((AVSampleFormat)kOutputAudioFormat));
		int ret = m_ptrSelectedDeckLinkOutput->WriteAudioSamplesSync(destData, 20, nullptr);
		delete[] destData;
		if (ret != S_OK)
		{
			LOG_ERROR("WriteAudioSamplesSync error");
		}
	}
}

QString VideoPlayback::onSignalChooseFileClicked()
{
	// 打开一个文件选择框，返回选择的文件，只选择mxf，mp4，mov格式的文件
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("Video Files (*.mxf *.mp4 *.mov)"));
	if (!fileName.isEmpty())
	{
		ui.fileNameLabel->setText(fileName);
	}
	m_strChooseFileName = fileName;
	return fileName;
}

void VideoPlayback::onSignalSliderValueChanged(double vlaue)
{
	qDebug() << "vlaue" << vlaue;
	// 触发时间小于200ms的不处理
	disconnect(ui.videoTImeSlider, &MySlider::signalSliderValueChanged, this, &VideoPlayback::onSignalSliderValueChanged);
	m_bSliderEnableConnect = false;
	double time = vlaue * ((ui.videoTImeSlider->maximum() - ui.videoTImeSlider->minimum()) + ui.videoTImeSlider->minimum()) / 100.0;
	m_ptrVideoDecoder->seekTo(time);
	QTimer::singleShot(100, this, [=]()
					   {
		m_bSliderEnableConnect = true;
		connect(ui.videoTImeSlider, &MySlider::signalSliderValueChanged, this, &VideoPlayback::onSignalSliderValueChanged); });
}

void VideoPlayback::onSignalSelectedDeviceChanged(int index)
{
	if (index < 0)
	{
		return;
	}
	QString strDeviceName = ui.deckLinkComboBox->itemText(index);
	auto iter = m_mapDeviceNameIndex.find(strDeviceName);
	if (iter == m_mapDeviceNameIndex.end())
	{
		return;
	}
	if (iter->second.m_bActive)
	{
#if defined(WIN32)
		m_ptrSelectedDeckLinkOutput = iter->second.deckLink;
#elif defined(__linux__)
		iter->second.deckLink->QueryInterface(IID_IDeckLinkOutput, (void **)&m_ptrSelectedDeckLinkOutput);
#endif
		initSDIOutput();
		LOG_INFO("Selected device is {}", strDeviceName.toStdString().c_str());
	}
	else
	{
		m_ptrSelectedDeckLinkOutput = nullptr;
	}
}

void VideoPlayback::customEvent(QEvent *event)
{
	switch (event->type())
	{
#if defined(__linux__)
	case kAddDeviceEvent:
	{
		DeckLinkDeviceDiscoveryEvent *addEvent = dynamic_cast<DeckLinkDeviceDiscoveryEvent *>(event);
		if (addEvent)
		{
			auto decklink = addEvent->deckLink();
			com_ptr<IDeckLinkProfileAttributes> decklinkAttributes(IID_IDeckLinkProfileAttributes, decklink);

			int64_t videoIOSupport;
			if (!decklinkAttributes)
			{
				qDebug() << "decklinkAttributes is null";
				return;
			}
			if (decklinkAttributes->GetInt(BMDDeckLinkVideoIOSupport, &videoIOSupport) != S_OK || (videoIOSupport & bmdDeviceSupportsPlayback) == 0)
			{
				qDebug() << "decklinkAttributes->GetInt(BMDDeckLinkVideoIOSupport, &videoIOSupport) != S_OK";
				return;
			}

			bool bActive = isDeviceActive(decklink);
			MyDeckLinkDevice device;
			device.deckLink = decklink;
			char *pName = nullptr;
			decklink->GetDisplayName((const char **)(&pName));
			device.m_strDisplayName = QString::fromLocal8Bit(pName);
			device.m_bActive = bActive;
			m_mapDeviceNameIndex[device.m_strDisplayName] = device;

			ui.deckLinkComboBox->addItem(device.m_strDisplayName);
		}
	}
	break;
#endif
	}
}

bool VideoPlayback::initConnect()
{
	bool flag = true;
	flag = flag && connect(ui.choosePushButton, &QPushButton::clicked, this, &VideoPlayback::onSignalChooseFileClicked);
	flag = flag && connect(this, &VideoPlayback::signalYUVData, ui.openGLWidget, &OpenGLPreviewWidget::onSignalYUVData);
	flag = flag && connect(ui.videoTImeSlider, &MySlider::signalSliderValueChanged, this, &VideoPlayback::onSignalSliderValueChanged);
	flag = flag && connect(ui.deckLinkComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoPlayback::onSignalSelectedDeviceChanged);
	return flag;
}

bool VideoPlayback::initAudioOutput()
{
	AudioInfo audioInfo;
	audioInfo.audioChannels = kOutputAudioChannels;
	audioInfo.audioSampleRate = kOutputAudioSampleRate;
	audioInfo.audioFormat = (AVSampleFormat)kOutputAudioFormat;
	return m_ptrAudioPlay->initOutputParameter(audioInfo) && m_ptrAudioPlay->startPlay();
}

bool VideoPlayback::initDecoder()
{
	if ("" == m_strChooseFileName)
	{
		return false;
	}
	VideoInfoAcqure::getInstance()->getVideoInfo(m_strChooseFileName.toStdString().c_str(), m_stuMediaInfo);
	setTimeSliderRange(m_stuMediaInfo.duration);
	updateTimeLabel(0, m_stuMediaInfo.duration);

	m_ptrVideoDecoder->unInitModule();
	VideoInfo videoInfo;
	videoInfo.width = kOutputVideoWidth;
	videoInfo.height = kOutputVideoHeight;
	videoInfo.videoFormat = (AVPixelFormat)kOutputVideoFormat;
	AudioInfo audioInfo;
	audioInfo.audioChannels = kOutputAudioChannels;
	audioInfo.audioSampleRate = kOutputAudioSampleRate;
	audioInfo.audioFormat = (AVSampleFormat)kOutputAudioFormat;
	m_ptrVideoDecoder->initModule(m_strChooseFileName.toStdString().c_str(), videoInfo, audioInfo);
	m_ptrVideoDecoder->initVideoCallBack(std::bind(&VideoPlayback::previewCallback, this, std::placeholders::_1, std::placeholders::_2), std::bind(&VideoPlayback::SDIOutputCallback, this, std::placeholders::_1));
	m_ptrVideoDecoder->initAudioCallback(std::bind(&VideoPlayback::AudioPlayCallBack, this, std::placeholders::_1, std::placeholders::_2));
	return true;
}

void VideoPlayback::updateTimeLabel(const int currentTime, const int totalTime)
{
	// 把int的秒数转为时分秒字符串
	int currentHour = currentTime / 3600;
	int currentMinute = (currentTime % 3600) / 60;
	int currentSecond = currentTime % 60;

	int totalHour = totalTime / 3600;
	int totalMinute = (totalTime % 3600) / 60;
	int totalSecond = totalTime % 60;

	QString strCurrentTime = QString("%1:%2:%3")
								 .arg(currentHour, 2, 10, QChar('0'))
								 .arg(currentMinute, 2, 10, QChar('0'))
								 .arg(currentSecond, 2, 10, QChar('0'));

	QString strTotalTime = QString("%1:%2:%3")
							   .arg(totalHour, 2, 10, QChar('0'))
							   .arg(totalMinute, 2, 10, QChar('0'))
							   .arg(totalSecond, 2, 10, QChar('0'));

	ui.timeCodeLabel->setText(strCurrentTime + "/" + strTotalTime);
}

void VideoPlayback::updateTimeSliderPosition(int64_t currentTime)
{
	if (ui.videoTImeSlider->getPressed())
	{
		return;
	}
	// QSlider修改值
	ui.videoTImeSlider->setValue(currentTime);
}

void VideoPlayback::setTimeSliderRange(int64_t totalTime)
{
	ui.videoTImeSlider->setRange(0, totalTime);
}

void VideoPlayback::initSDIOutput()
{
	if (nullptr == m_ptrSelectedDeckLinkOutput)
	{
		return;
	}
	// 初始化SDI输出
	m_ptrSelectedDeckLinkOutput->EnableVideoOutput((BMDDisplayMode)kSDIOutputFormat, bmdVideoOutputFlagDefault);
	m_ptrSelectedDeckLinkOutput->EnableAudioOutput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, kOutputAudioChannels, bmdAudioOutputStreamContinuous);
	if (nullptr == kDecklinkOutputFrame)
	{
		m_ptrSelectedDeckLinkOutput->CreateVideoFrame(kOutputVideoWidth, kOutputVideoHeight, kOutputVideoWidth * 2, bmdFormat8BitYUV, bmdFrameFlagDefault, &kDecklinkOutputFrame);
	}
}
