#include "VideoPlayback.h"
#include "ui/MySlider/MySlider.h"
#include "module/VideoInfo/VideoInfoAcqure.h"
#include "module/VideoDecoder/HardDecoder.h"
#include "module/utils/utils.h"

#if defined(WIN32)
	#include "module/AtomDecoder/avid_mxf_info.h"
#endif

#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QFileDialog>

#include <sstream>
#include <iomanip>

#if defined(WIN32)
std::string umidToString(const mxfUMID* umid) 
{
	std::stringstream ss;
	ss << std::hex << std::setfill('0');

	const unsigned char* bytes = reinterpret_cast<const unsigned char*>(umid);
	for (int i = 0; i < 32; ++i) {
		ss << std::setw(2) << static_cast<int>(bytes[i]);
	}
	return ss.str();
}
#endif

#if defined(BlackMagicEnabled)
	IDeckLinkMutableVideoFrame* kDecklinkOutputFrame = nullptr;
#endif(BlackMagicEnabled)

//std::fstream fs("audio.pcm1", std::ios::out | std::ios::binary);
//std::fstream fs1("audio.pcm2", std::ios::out | std::ios::binary);
VideoPlayback::VideoPlayback(QWidget* parent)
	: QWidget(parent)
{
	ui.setupUi(this);

	initModule();

	connect(ui.startPushButton, &QPushButton::clicked, this, [=]()
		{
			if (ui.atomRadioButton->isChecked())
			{
				if (m_ptrAtomDecoder)
				{
					delete m_ptrAtomDecoder;
					m_ptrAtomDecoder = nullptr;
				}
				initAudioOutput();
				m_ptrAtomDecoder = new AtomDecoder();
				if (initAllModule())
				{
					m_ptrAtomDecoder->startDecoder();
				}
			}
			else
			{
				uninitAllModule();
				initAudioOutput();
				if (!initAllModule())
				{
				}
			}
		});

	connect(ui.pausePushButton, &QPushButton::clicked, this, [=]()
		{
			if (!ui.atomRadioButton->isChecked())
				if (m_ptrPreviewAndPlay)
				{
					m_ptrPreviewAndPlay->pause();
				}
		});

	connect(ui.continuePushButton, &QPushButton::clicked, this, [=]()
		{
			if (m_ptrPreviewAndPlay)
			{
				m_ptrPreviewAndPlay->resume();
			}
		});

	connect(ui.atomRadioButton, &QRadioButton::clicked, this, [=]() 
		{
			if (ui.atomRadioButton->isChecked())
			{
				ui.pausePushButton->hide();
				ui.continuePushButton->hide();
			}
			else
			{
				ui.pausePushButton->show();
				ui.continuePushButton->show();
			}
		});
}

VideoPlayback::~VideoPlayback()
{
	//fs.close();
	//fs1.close();
	uninitAllModule();

	if (m_ptrAtomDecoder)
	{
		delete m_ptrAtomDecoder;
		m_ptrAtomDecoder = nullptr;
	}
	if (m_ptrAudioPlay)
	{
		delete m_ptrAudioPlay;
		m_ptrAudioPlay = nullptr;
	}
}

bool VideoPlayback::initModule()
{
	m_eDeviceType = utils::getSupportedHWDeviceType();
	if (AV_HWDEVICE_TYPE_NONE == m_eDeviceType)
	{
		LOG_ERROR("No supported hardware device found");
		return false;
	}
	//m_ptrAtomDecoder = new AtomDecoder();
	m_ptrAudioPlay = new AudioPlay(nullptr);

#if defined(BlackMagicEnabled)
#if defined(WIN32)
	m_ptrDeckLinkDeviceDiscovery = new DeckLinkDeviceDiscovery();
	m_ptrDeckLinkDeviceDiscovery->OnDeviceArrival(std::bind(&VideoPlayback::deviceDiscovered, this, std::placeholders::_1));
#elif defined(__linux__)
	m_ptrDeckLinkDeviceDiscovery = new DeckLinkDeviceDiscovery(this);
#endif
	m_ptrDeckLinkDeviceDiscovery->Enable();
#endif(BlackMagicEnabled)

	m_stuVideoInfo.width = kOutputVideoWidth;
	m_stuVideoInfo.height = kOutputVideoHeight;
	m_stuVideoInfo.videoFormat = (AVPixelFormat)kOutputVideoFormat;

	m_stuAudioInfo.audioChannels = kOutputAudioChannels;
	m_stuAudioInfo.audioSampleRate = kOutputAudioSampleRate;
	m_stuAudioInfo.audioFormat = (AVSampleFormat)kOutputAudioFormat;
	m_stuAudioInfo.samplePerChannel = kOutputAudioSamplePerChannel;

	return initConnect();
}

void VideoPlayback::previewCallback(std::shared_ptr<VideoCallbackInfo> videoInfo)
{
	// ��YUV���ݷ��ͳ�ȥ
	if (nullptr == videoInfo->yuvData)
	{
		return;
	}
	VideoInfo video;
	video.width = videoInfo->width;
	video.height = videoInfo->height;
	video.videoFormat = videoInfo->videoFormat;
	QByteArray data((char*)videoInfo->yuvData, videoInfo->dataSize);
	emit signalYUVData(data, video);
	updateTimeLabel(videoInfo->m_dPts, m_stuMediaInfo.duration);
	updateTimeSliderPosition(videoInfo->m_dPts);
}

void VideoPlayback::audioCallback(std::vector<Buffer*> audioBuffer)
{
	//��buffer�а�˳��ȡ��pcm���ݣ�Ȼ���ս���ķ�ʽ�������������
	int channelNum = (int)audioBuffer.size();
	uint8_t* pcmData = new uint8_t[channelNum * m_uiAtomAudioSampleCntPerFrame * 2]{ 0 };

	std::vector<uint8_t*> vecAudioBuffer;
	for (int i = 0; i < channelNum; ++i)
	{
		uint8_t* pSrc = new uint8_t[m_uiAtomAudioSampleCntPerFrame * 2]{ 0 };
		audioBuffer[i]->getBuffer(pSrc, m_uiAtomAudioSampleCntPerFrame * 2);
		if (0 == i)
		{
			QByteArray data((char*)pSrc, m_uiAtomAudioSampleCntPerFrame * 2);
			m_ptrAudioPlay->inputPcmData(data);
		}
		vecAudioBuffer.push_back(pSrc);
	}

	//for (int i = 0; i < 1920; ++i)
	//{
	//	//16λ�����ʵ���Ƶ���ݣ���ϳɽ���ķ�ʽ
	//	for (size_t j = 0; j < channelNum; ++j)
	//	{
	//		pcmData[(i * channelNum + j) * 2] = vecAudioBuffer[j][i * 2];
	//	}
	//}
	memcpy(pcmData, vecAudioBuffer[0], m_uiAtomAudioSampleCntPerFrame * 2);
	memcpy(pcmData + m_uiAtomAudioSampleCntPerFrame * 2, vecAudioBuffer[1], m_uiAtomAudioSampleCntPerFrame * 2);
	//fs.write((char*)vecAudioBuffer[0], 1920 * 2);
	//fs1.write((char*)pcmData, 1920 * 4);
	for (auto& item : vecAudioBuffer)
	{
		delete[]item;
	}
	//���浽����
	delete pcmData;
}

void VideoPlayback::SDIOutputCallback(const VideoCallbackInfo& videoInfo)
{
	if (nullptr == videoInfo.yuvData || nullptr == m_ptrSelectedDeckLinkOutput)
	{
		return;
	}
	uint8_t* destData = nullptr;
	kDecklinkOutputFrame->GetBytes((void**)&destData);
	std::copy(videoInfo.yuvData, videoInfo.yuvData + videoInfo.dataSize, destData);
	std::unique_lock<std::mutex> lck(m_mutex);
	int ret = m_ptrSelectedDeckLinkOutput->DisplayVideoFrameSync(kDecklinkOutputFrame);
	lck.unlock();
	if (ret != S_OK)
	{
		LOG_ERROR("DisplayVideoFrameSync error");
	}
}

void VideoPlayback::audioPlayCallBack(std::shared_ptr<AudioCallbackInfo> audioInfo)
{
	//int cvafd = kOutputAudioChannels * channelSampleNumber * av_get_bytes_per_sample((AVSampleFormat)kOutputAudioFormat);
	QByteArray data((char*)audioInfo->m_pPCMData, audioInfo->m_ulPCMLength);
	m_ptrAudioPlay->inputPcmData(data);

	//TODO����ժ����
#if defined(BlackMagicEnabled)
	if (nullptr != m_ptrSelectedDeckLinkOutput)
	{
		uint8_t* destData = nullptr;
		uint32_t iWritten = 0;
		std::unique_lock<std::mutex> lck(m_mutex);
		uint64_t ret = m_ptrSelectedDeckLinkOutput->WriteAudioSamplesSync(audioInfo->m_pPCMData, audioInfo->m_ulPCMLength, &iWritten);
		lck.unlock();
		if (ret != S_OK)
		{
			LOG_ERROR("WriteAudioSamplesSync error,ret={}", ret);
		}
		if (iWritten != audioInfo->m_ulPCMLength)
		{
			LOG_ERROR("WriteAudioSamplesSync error,iWritten={},dest cnt={}", iWritten, audioInfo->m_ulPCMLength);
		}
	}
#endif(BlackMagicEnabled)
}

QString VideoPlayback::onSignalChooseFileClicked()
{
	if (ui.atomRadioButton->isChecked())
	{
		m_bAtomFileValid = true;
		//��һ���ļ�ѡ���ѡ�����ļ���ֻѡ��mxf�ļ�
		QStringList fileNames = QFileDialog::getOpenFileNames(this, tr("Open File"), "", tr("Video Files (*.mxf)"));
		if (!fileNames.isEmpty())
		{
			ui.fileNameLabel->setText(fileNames.first());
		}
		else
		{
			return "";
		}
		int vNum = 0;

		std::string strMaterialId = "";

		for (auto& item : fileNames)
		{
#if defined(WIN32)
			AvidMXFInfo mxfInfo;

			auto datad = item.toLocal8Bit();
			std::string strFilePath(datad.constData(), datad.length());
			int result = ami_read_info(strFilePath.c_str(), &mxfInfo, 1);

			if (strMaterialId.empty())
			{
				strMaterialId = umidToString(&mxfInfo.materialPackageUID);
			}
			else
			{
				if (strMaterialId != umidToString(&mxfInfo.materialPackageUID))
				{
					m_strChooseFileName.clear();
					m_vecChooseNameAtom.clear();
					MyTipDialog::show(QString::fromLocal8Bit("error"), QString::fromLocal8Bit("�زĴ���"), true);
					return "";
				}
			}

			ami_free_info(&mxfInfo);
#endif
			MediaInfo mediaInfo;
			VideoInfoAcqure::getInstance()->getVideoInfo(item.toStdString().c_str(), mediaInfo);
			if (MediaType::VideoAndAudio == mediaInfo.mediaType)
			{
				m_bAtomFileValid = false;
				LOG_ERROR("The file not atom format{}", item.toStdString().c_str());
				m_strChooseFileName.clear();
				m_vecChooseNameAtom.clear();
				MyTipDialog::show(QString::fromLocal8Bit("error"), QString::fromLocal8Bit("��atom�ز�"), true);
				return "";
			}
			else if (MediaType::Video == mediaInfo.mediaType)
			{
				if (++vNum > 1)
				{
					m_bAtomFileValid = false;
					m_strChooseFileName.clear();
					m_vecChooseNameAtom.clear();
					LOG_ERROR("file contains more than two video file");
					MyTipDialog::show(QString::fromLocal8Bit("error"), QString::fromLocal8Bit("��Ƶ�ز�����"), true);
					return "";
				}
				m_uiAtomFrameCnt = mediaInfo.fps;
				m_strChooseFileName = item;
			}
			else if (MediaType::Audio == mediaInfo.mediaType)
			{
				m_vecChooseNameAtom.push_back(item);
				m_uiAtomAudioSampleRate = mediaInfo.audioSampleRate;
			}
		}
		m_uiAtomAudioSampleCntPerFrame = m_uiAtomAudioSampleRate / m_uiAtomFrameCnt;
		return fileNames.first();
	}
	else
	{
		// ��һ���ļ�ѡ��򣬷���ѡ����ļ���ֻѡ��mxf��mp4��mov��ʽ���ļ�
		QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("Video Files (*.mxf *.mp4 *.mov)"));
		if (!fileName.isEmpty())
		{
			ui.fileNameLabel->setText(fileName);
		}
		m_strChooseFileName = fileName;
		return fileName;
	}
}

void VideoPlayback::onSignalSliderValueChanged(double vlaue)
{
	// ����ʱ��С��200ms�Ĳ�����
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
		iter->second.deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&m_ptrSelectedDeckLinkOutput);
#endif
		initSDIOutput();
		LOG_INFO("Selected device is {}", strDeviceName.toStdString().c_str());
	}
	else
	{
		m_ptrSelectedDeckLinkOutput = nullptr;
	}
}

void VideoPlayback::customEvent(QEvent* event)
{
#if defined(__linux__)
	switch (event->type())
	{
	case kAddDeviceEvent:
	{
		DeckLinkDeviceDiscoveryEvent* addEvent = dynamic_cast<DeckLinkDeviceDiscoveryEvent*>(event);
		if (addEvent)
		{
			auto decklink = addEvent->deckLink();
			com_ptr<IDeckLinkProfileAttributes> decklinkAttributes(IID_IDeckLinkProfileAttributes, decklink);

			int64_t videoIOSupport;
			if (!decklinkAttributes)
			{
				LOG_ERROR("decklinkAttributes is null");
				return;
			}
			if (decklinkAttributes->GetInt(BMDDeckLinkVideoIOSupport, &videoIOSupport) != S_OK || (videoIOSupport & bmdDeviceSupportsPlayback) == 0)
			{
				LOG_ERROR("decklinkAttributes->GetInt(BMDDeckLinkVideoIOSupport, &videoIOSupport) != S_OK");
				return;
			}

			bool bActive = isDeviceActive(decklink);
			MyDeckLinkDevice device;
			device.deckLink = decklink;
			char* pName = nullptr;
			decklink->GetDisplayName((const char**)(&pName));
			device.m_strDisplayName = QString::fromLocal8Bit(pName);
			device.m_bActive = bActive;
			m_mapDeviceNameIndex[device.m_strDisplayName] = device;

			ui.deckLinkComboBox->addItem(device.m_strDisplayName);
		}
	}
	break;
	}
#endif
}

bool VideoPlayback::initConnect()
{
	bool flag = true;
	flag = flag && connect(ui.choosePushButton, &QPushButton::clicked, this, &VideoPlayback::onSignalChooseFileClicked);
	flag = flag && connect(this, &VideoPlayback::signalYUVData, ui.openGLWidget, &OpenGLPreviewWidget::onSignalYUVData, Qt::QueuedConnection);
	flag = flag && connect(ui.videoTImeSlider, &MySlider::signalSliderValueChanged, this, &VideoPlayback::onSignalSliderValueChanged);
	flag = flag && connect(ui.deckLinkComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoPlayback::onSignalSelectedDeviceChanged);
	return flag;
}

bool VideoPlayback::initAudioOutput()
{
	m_ptrAudioPlay->clearAudioDevice();
	AudioInfo audioInfo;
	audioInfo.audioChannels = 2;
	audioInfo.audioSampleRate = kOutputAudioSampleRate;
	audioInfo.audioFormat = (AVSampleFormat)kOutputAudioFormat;
	return m_ptrAudioPlay->initOutputParameter(audioInfo) && m_ptrAudioPlay->startPlay();
}

bool VideoPlayback::initAllModule()
{
	if ("" == m_strChooseFileName)
	{
		return false;
	}
	VideoInfoAcqure::getInstance()->getVideoInfo(m_strChooseFileName.toStdString().c_str(), m_stuMediaInfo);
	setTimeSliderRange(m_stuMediaInfo.duration);
	updateTimeLabel(0, m_stuMediaInfo.duration);

	if (ui.atomRadioButton->isChecked())
	{
		m_ptrAtomDecoder->unInitModule();
		VideoInfo videoInfo;
		videoInfo.width = kOutputVideoWidth;
		videoInfo.height = kOutputVideoHeight;
		videoInfo.videoFormat = (AVPixelFormat)kOutputVideoFormat;
		std::vector<std::pair<std::string, AudioInfo>> vecTmp;
		for (auto& item : m_vecChooseNameAtom)
		{
			AudioInfo audioInfo;
			audioInfo.audioChannels = kAtomOutputAudioChannels;
			audioInfo.audioSampleRate = kOutputAudioSampleRate;
			audioInfo.audioFormat = (AVSampleFormat)kOutputAudioFormat;
			audioInfo.samplePerChannel = kOutputAudioSamplePerChannel;
			vecTmp.push_back(std::pair<std::string, AudioInfo>(item.toStdString(), audioInfo));
		}
		m_ptrAtomDecoder->initModule(m_strChooseFileName.toStdString().c_str(), vecTmp, videoInfo);
		//m_ptrAtomDecoder->initVideoCallBack(std::bind(&VideoPlayback::previewCallback, this, std::placeholders::_1, std::placeholders::_2), std::bind(&VideoPlayback::SDIOutputCallback, this, std::placeholders::_1));
		//m_ptrAtomDecoder->initAudioCallback(std::bind(&VideoPlayback::AudioCallback, this, std::placeholders::_1));
	}
	else
	{
		auto videoWaitDecodedQueue = std::make_shared<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>>();
		auto videoAfterDecodedQueue = std::make_shared<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>>();
		auto ptrPcmBuffer = std::make_shared<Buffer>();


		videoWaitDecodedQueue->initModule();
		videoAfterDecodedQueue->initModule();
		ptrPcmBuffer->initBuffer(1024 * 10);

		m_ptrVideoReader = std::make_shared<VideoReader>();
		m_eDeviceType = AV_HWDEVICE_TYPE_NONE;
		if (AV_HWDEVICE_TYPE_NONE == m_eDeviceType)
		{
			m_ptrVideoDecoder = std::make_shared<VideoDecoder>(m_ptrVideoReader);
		}
		else
		{
			m_ptrVideoDecoder = std::make_shared< HardDecoder>(m_ptrVideoReader);
		}
		m_ptrPreviewAndPlay = std::make_shared<PreviewAndPlay>();

		VideoReaderInitedInfo videoInitedInfo;
		DecoderInitedInfo decoderInitedInfo;
		DataHandlerInitedInfo dataHandlerInfo;

		videoInitedInfo.m_strFileName = m_strChooseFileName.toLocal8Bit().constData();
		videoInitedInfo.m_eDeviceType = m_eDeviceType;
		videoInitedInfo.outVideoInfo = m_stuVideoInfo;
		videoInitedInfo.outAudioInfo = m_stuAudioInfo;
		videoInitedInfo.ptrPacketQueue = videoWaitDecodedQueue;
		videoInitedInfo.m_bAtom = false;

		m_ptrVideoReader->initModule(videoInitedInfo, decoderInitedInfo);

		m_ptrVideoDecoder->addPacketQueue(videoAfterDecodedQueue);
		m_ptrVideoDecoder->addPCMBuffer(ptrPcmBuffer);
		m_ptrVideoDecoder->initModule(decoderInitedInfo, dataHandlerInfo);
		m_ptrVideoDecoder->registerFinishedCallback(std::bind(&VideoPlayback::onDecoderFinshed, this));

		m_ptrPreviewAndPlay->setVideoQueue(videoAfterDecodedQueue);
		m_ptrPreviewAndPlay->setAudioQueue(ptrPcmBuffer);
		m_ptrPreviewAndPlay->setCallback(std::bind(&VideoPlayback::previewCallback, this, std::placeholders::_1));
		m_ptrPreviewAndPlay->setCallback(std::bind(&VideoPlayback::audioPlayCallBack, this, std::placeholders::_1));
		m_ptrPreviewAndPlay->setFinishedCallback(std::bind(&VideoPlayback::onConsumeFinished, this));
		m_ptrPreviewAndPlay->initModule(dataHandlerInfo);
	}
	return true;
}

bool VideoPlayback::uninitAllModule()
{
	if (m_ptrVideoReader)
	{
		//m_ptrVideoReader->uninitModule();
		m_ptrVideoReader = nullptr;
	}
	if (m_ptrVideoDecoder)
	{
		//m_ptrVideoDecoder->uninitModule();
		m_ptrVideoDecoder = nullptr;
	}
	if (m_ptrPreviewAndPlay)
	{
		//m_ptrPreviewAndPlay->uninitModule();
		m_ptrPreviewAndPlay = nullptr;
	}
	return 0;
}

void VideoPlayback::updateTimeLabel(const int currentTime, const int totalTime)
{
	// ��int������תΪʱ�����ַ���
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
	// QSlider�޸�ֵ
	ui.videoTImeSlider->setValue(currentTime);
}

void VideoPlayback::setTimeSliderRange(int64_t totalTime)
{
	ui.videoTImeSlider->setRange(0, totalTime);
}

void VideoPlayback::onDecoderFinshed()
{
	m_ptrPreviewAndPlay->setDecoderFinshedState(true);
}

void VideoPlayback::onConsumeFinished()
{
	std::thread t1([this]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		uninitAllModule();
		});
	t1.detach();
}

#if defined(BlackMagicEnabled)
void VideoPlayback::initSDIOutput()
{
	if (nullptr == m_ptrSelectedDeckLinkOutput)
	{
		return;
	}
	std::unique_lock<std::mutex> lck(m_mutex);
	// ��ʼ��SDI���
	int64_t ret = m_ptrSelectedDeckLinkOutput->EnableVideoOutput((BMDDisplayMode)kSDIOutputFormat, bmdVideoOutputFlagDefault);
	if (S_OK != ret)
	{
		LOG_ERROR("enable video output failed,ret={}", ret);
	}
	ret = m_ptrSelectedDeckLinkOutput->EnableAudioOutput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, kOutputAudioChannels, bmdAudioOutputStreamContinuous);
	if (S_OK != ret)
	{
		LOG_ERROR("enable audio output failed,ret={}", ret);
	}
	if (nullptr == kDecklinkOutputFrame)
	{
		m_ptrSelectedDeckLinkOutput->CreateVideoFrame(kOutputVideoWidth, kOutputVideoHeight, kOutputVideoWidth * 2, bmdFormat8BitYUV, bmdFrameFlagDefault, &kDecklinkOutputFrame);
	}
	lck.unlock();
}

#if defined(WIN32)
	void VideoPlayback::deviceDiscovered(CComPtr<IDeckLink>& deckLink)
	{
		CComBSTR deviceNameBSTR = nullptr;
		HRESULT hr;

		hr = deckLink->GetDisplayName(&deviceNameBSTR);
		if (FAILED(hr))
		{
			// �������
			return;
		}

		std::wstring deviceNameWStr = BSTRToWString(deviceNameBSTR.m_str);
		std::string deviceNameStr = WStringToString(deviceNameWStr);
		QString deviceName = QString::fromStdString(deviceNameStr);

		// bool bActive = isDeviceActive(deckLink);
		MyDeckLinkDevice device;
		device.deckLink = deckLink;
		device.m_strDisplayName = deviceName;
		device.m_bActive = true;
		m_mapDeviceNameIndex[device.m_strDisplayName] = device;

		ui.deckLinkComboBox->addItem(device.m_strDisplayName);
	}
#endif(WIN32)

#endif(BlackMagicEnabled)
