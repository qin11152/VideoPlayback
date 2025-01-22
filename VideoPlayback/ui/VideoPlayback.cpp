#include "VideoPlayback.h"
#include "ui/MySlider/MySlider.h"
#include "module/VideoInfo/VideoInfoAcqure.h"
#include "module/VideoDecoder/HardDecoder.h"
#include "module/VideoDecoder/AtomDecoder.h"
#include "module/utils/utils.h"

#if defined(WIN32)
#include "module/MXF++/avid_mxf_info.h"
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
#endif //BlackMagicEnabled

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
				uninitAllSubModule();
				if (!initAllSubModule())
				{
				}
			}
			else
			{
				uninitAllSubModule();
				if (!initAllSubModule())
				{
				}
			}
		});

	connect(ui.pausePushButton, &QPushButton::clicked, this, [=]()
		{
			if (!ui.atomRadioButton->isChecked())
			{
				if (m_ptrPreviewAndPlay)
				{
					m_ptrPreviewAndPlay->pause();
				}
			}
			else
			{
				if (m_ptrAtomPreviewAndPlay)
				{
					m_ptrAtomPreviewAndPlay->pause();
				}
			}
		});

	connect(ui.continuePushButton, &QPushButton::clicked, this, [=]()
		{
			if (!ui.atomRadioButton->isChecked())
			{
				if (m_ptrPreviewAndPlay)
				{
					m_ptrPreviewAndPlay->resume();
				}
			}
			else
			{
				if (m_ptrAtomPreviewAndPlay)
				{
					m_ptrAtomPreviewAndPlay->resume();
				}
			}
		});

	connect(ui.atomRadioButton, &QRadioButton::clicked, this, [=]()
		{
			if (ui.atomRadioButton->isChecked())
			{
				//ui.pausePushButton->hide();
				//ui.continuePushButton->hide();
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
	uninitAllSubModule();
}

bool VideoPlayback::initModule()
{
	m_eDeviceType = utils::getSupportedHWDeviceType();
	if (AV_HWDEVICE_TYPE_NONE == m_eDeviceType)
	{
		LOG_ERROR("No supported hardware device found");
		return false;
	}
	else
	{
		LOG_INFO("Choose Device Type:{}", av_hwdevice_get_type_name(m_eDeviceType));
	}

#if defined(BlackMagicEnabled)
#if defined(WIN32)
	m_ptrDeckLinkDeviceDiscovery = new DeckLinkDeviceDiscovery();
	m_ptrDeckLinkDeviceDiscovery->OnDeviceArrival(std::bind(&VideoPlayback::deviceDiscovered, this, std::placeholders::_1));
#elif defined(__linux__)
	m_ptrDeckLinkDeviceDiscovery = new DeckLinkDeviceDiscovery(this);
#endif //(WIN32)
	m_ptrDeckLinkDeviceDiscovery->Enable();
#endif //BlackMagicEnabled

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
	// 将YUV数据发送出去
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

void VideoPlayback::audioPlayCallBack(std::shared_ptr<AudioCallbackInfo> audioInfo)
{
	//int cvafd = kOutputAudioChannels * channelSampleNumber * av_get_bytes_per_sample((AVSampleFormat)kOutputAudioFormat);
	QByteArray data((char*)audioInfo->m_pPCMData, audioInfo->m_ulPCMLength);
	inputPcmData(data);

	//TODO单独摘出来
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
#endif
}

void VideoPlayback::SDIOutputCallback(const VideoCallbackInfo& videoInfo)
{
#if defined(BlackMagicEnabled)
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
#endif
}

void VideoPlayback::atomAudioCallback(std::shared_ptr<AudioCallbackInfo> audioInfo)
{
	if (!audioInfo->m_bAtom)
	{
		return;
	}
	//从buffer中按顺序取出pcm数据，然后按照交错的方式将数据组合起来
	uint8_t* pcmData = new uint8_t[audioInfo->m_ulPCMLength * audioInfo->m_vecPcmData.size()]{ 0 };

	std::vector<uint8_t*> vecAudioBuffer;
	for (int i = 0; i < audioInfo->m_vecPcmData.size(); ++i)
	{
		vecAudioBuffer.push_back(audioInfo->m_vecPcmData[i]);
		if (0 == i)
		{
			QByteArray data((char*)audioInfo->m_vecPcmData[i], audioInfo->m_ulPCMLength);
			inputPcmData(data);
		}
	}

	//for (int i = 0; i < 1920; ++i)
	//{
	//	//16位采样率的音频数据，组合成交错的方式
	//	for (size_t j = 0; j < channelNum; ++j)
	//	{
	//		pcmData[(i * channelNum + j) * 2] = vecAudioBuffer[j][i * 2];
	//	}
	//}
	for (int i = 0; i < vecAudioBuffer.size(); ++i)
	{
		memcpy(pcmData + i * m_uiAtomAudioSampleCntPerFrame * 2, vecAudioBuffer[i], m_uiAtomAudioSampleCntPerFrame * 2);
	}
	//QByteArray data((char*)pcmData, audioInfo->m_ulPCMLength * audioInfo->m_vecPcmData.size());
	//inputPcmData(data);
	delete []pcmData;
}

QString VideoPlayback::onSignalChooseFileClicked()
{
	if (ui.atomRadioButton->isChecked())
	{
		m_bAtomFileValid = true;
		//打开一个文件选择框，选择多个文件，只选择mxf文件
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
					MyTipDialog::show(QString::fromLocal8Bit("error"), QString::fromLocal8Bit("素材错误"), true);
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
				MyTipDialog::show(QString::fromLocal8Bit("error"), QString::fromLocal8Bit("非atom素材"), true);
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
					MyTipDialog::show(QString::fromLocal8Bit("error"), QString::fromLocal8Bit("音频素材有误"), true);
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
		// 打开一个文件选择框，返回选择的文件，只选择mxf，mp4，mov格式的文件
		QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("Video Files (*.mxf *.mp4 *.mov *.venc)"));
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
	// 触发时间小于200ms的不处理
	disconnect(ui.videoTImeSlider, &MySlider::signalSliderValueChanged, this, &VideoPlayback::onSignalSliderValueChanged);
	m_bSliderEnableConnect = false;
	double time = vlaue * ((ui.videoTImeSlider->maximum() - ui.videoTImeSlider->minimum()) + ui.videoTImeSlider->minimum()) / 100.0;
	if (m_ptrVideoDecoder)
	{
		m_ptrVideoDecoder->seekTo(time);
	}
	QTimer::singleShot(100, this, [=]()
		{
			m_bSliderEnableConnect = true;
			connect(ui.videoTImeSlider, &MySlider::signalSliderValueChanged, this, &VideoPlayback::onSignalSliderValueChanged); });
}

void VideoPlayback::onSignalSelectedDeviceChanged(int index)
{
#if defined(BlackMagicEnabled)
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
#endif
}

void VideoPlayback::customEvent(QEvent* event)
{
#if defined(__linux__)
	#if defined(BlackMagicEnabled)
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
	clearAudioDevice();
	AudioInfo audioInfo;
	if (ui.atomRadioButton->isChecked())
	{
		audioInfo.audioChannels = kAtomOutputAudioChannel;
	}
	else
	{
		audioInfo.audioChannels = kOutputAudioChannels;
	}
	audioInfo.audioSampleRate = kOutputAudioSampleRate;
	audioInfo.audioFormat = (AVSampleFormat)kOutputAudioFormat;
	return initOutputParameter(audioInfo) && startPlay();
}

bool VideoPlayback::initAllSubModule()
{
	if ("" == m_strChooseFileName)
	{
		return false;
	}
	VideoInfoAcqure::getInstance()->getVideoInfo(m_strChooseFileName.toStdString().c_str(), m_stuMediaInfo);
	setTimeSliderRange(m_stuMediaInfo.duration);
	updateTimeLabel(0, m_stuMediaInfo.duration);
	initAudioOutput();
	m_uiConsumeCallbackCnt = 0;
	if (ui.atomRadioButton->isChecked())
	{
		auto videoWaitDecodedQueue = std::make_shared<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>>();
		std::vector<std::shared_ptr<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>>> vecAudioWaitedDecodedQueue;

		auto vecPCMBuffer = std::make_shared<std::vector<std::shared_ptr<Buffer>>>();
		auto videoAfterDecodedQueue = std::make_shared<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>>();

		videoWaitDecodedQueue->initModule();
		videoAfterDecodedQueue->initModule();

		for (int i = 0; i < m_vecChooseNameAtom.size(); ++i)
		{
			auto tmpPcmBuffer = std::make_shared<Buffer>();
			tmpPcmBuffer->initBuffer(1024 * 10);
			vecPCMBuffer->push_back(tmpPcmBuffer);

			auto tmpAudioQueue = std::make_shared<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>>();
			tmpAudioQueue->initModule();
			vecAudioWaitedDecodedQueue.push_back(tmpAudioQueue);
		}

		//std::vector<VideoReaderInitedInfo> vecVideoInitedInfo;
		DecoderInitedInfo decoderInitedInfo;
		DataHandlerInitedInfo dataHandlerInfo;

		m_ptrAtomPreviewAndPlay = std::make_shared<AtomPreviewAndPlay>();

		VideoReaderInitedInfo info;
		info.m_bAtom = true;
		info.m_eDeviceType = m_eDeviceType;
		info.m_strFileName = m_strChooseFileName.toStdString();
		info.outAudioInfo = m_stuAudioInfo;
		info.outVideoInfo = m_stuVideoInfo;
		info.ptrPacketQueue = videoWaitDecodedQueue;
		m_ptrVideoReader = std::make_shared<VideoReader>();
		m_ptrVideoReader->initModule(info, decoderInitedInfo);
		//vecVideoInitedInfo.push_back(info);

		for (int i = 0; i < m_vecChooseNameAtom.size(); ++i)
		{
			VideoReaderInitedInfo info;
			info.m_bAtom = true;
			info.m_eDeviceType = m_eDeviceType;
			info.m_strFileName = m_vecChooseNameAtom[i].toStdString();
			info.outAudioInfo = m_stuAudioInfo;
			info.outVideoInfo = m_stuVideoInfo;
			info.ptrPacketQueue = vecAudioWaitedDecodedQueue[i];
			auto tmp = std::make_shared<VideoReader>();
			tmp->initModule(info, decoderInitedInfo);
			m_vecVideoReader.push_back(tmp);
			//vecVideoInitedInfo.push_back(info);
		}

		decoderInitedInfo.outAudioInfo = m_stuAudioInfo;
		decoderInitedInfo.outVideoInfo = m_stuVideoInfo;
		decoderInitedInfo.ptrAtomVideoPacketQueue = videoWaitDecodedQueue;
		decoderInitedInfo.vecAtomAudioPacketQueue = vecAudioWaitedDecodedQueue;
		decoderInitedInfo.m_eDeviceType = m_eDeviceType;
		decoderInitedInfo.m_bAtom = true;

		m_ptrVideoDecoder = std::make_shared<AtomDecoder>(m_ptrVideoReader, m_vecVideoReader);

		m_ptrVideoDecoder->addAtomVideoPacketQueue(videoAfterDecodedQueue);
		m_ptrVideoDecoder->addAtomAudioPacketQueue(vecPCMBuffer);
		m_ptrVideoDecoder->initModule(decoderInitedInfo, dataHandlerInfo);
		m_ptrVideoDecoder->registerFinishedCallback(std::bind(&VideoPlayback::onDecoderFinshed, this));

		m_ptrAtomPreviewAndPlay->setVideoQueue(videoAfterDecodedQueue);
		m_ptrAtomPreviewAndPlay->setAudioQueue(vecPCMBuffer);
		m_ptrAtomPreviewAndPlay->setCallback(std::bind(&VideoPlayback::previewCallback, this, std::placeholders::_1));
		m_ptrAtomPreviewAndPlay->setCallback(std::bind(&VideoPlayback::atomAudioCallback, this, std::placeholders::_1));
		m_ptrAtomPreviewAndPlay->setFinishedCallback(std::bind(&VideoPlayback::onConsumeFinished, this));
		m_ptrAtomPreviewAndPlay->initModule(dataHandlerInfo);

		m_uiConsumeCnt = 1;
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
		if (AV_HWDEVICE_TYPE_NONE == m_eDeviceType)
		{
			m_ptrVideoDecoder = std::make_shared<VideoDecoder>(m_ptrVideoReader);
		}
		else
		{
			LOG_INFO("Use Hardware Decoder");
			m_ptrVideoDecoder = std::make_shared< HardDecoder>(m_ptrVideoReader);
		}
		m_ptrPreviewAndPlay = std::make_shared<PreviewAndPlay>();

		VideoReaderInitedInfo videoInitedInfo;
		DecoderInitedInfo decoderInitedInfo;
		DataHandlerInitedInfo dataHandlerInfo;

		videoInitedInfo.m_strFileName = m_strChooseFileName.toStdString();
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

		m_uiConsumeCnt = 1;
	}
	return true;
}

bool VideoPlayback::uninitAllSubModule()
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

	if (m_vecVideoReader.size() > 0)
	{
		for (auto& item : m_vecVideoReader)
		{
			//item->uninitModule();
		}
		m_vecVideoReader.clear();
	}

	if (m_ptrAtomPreviewAndPlay)
	{
		//m_ptrAtomPreviewAndPlay->uninitModule();
		m_ptrAtomPreviewAndPlay = nullptr;
	}
	return 0;
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

void VideoPlayback::onDecoderFinshed()
{
	if (m_ptrPreviewAndPlay)
	{
		m_ptrPreviewAndPlay->setDecoderFinshedState(true);
	}
	if (m_ptrAtomPreviewAndPlay)
	{
		m_ptrAtomPreviewAndPlay->setDecoderFinshedState(true);
	}
}

void VideoPlayback::onConsumeFinished()
{
	if (++m_uiConsumeCallbackCnt == m_uiConsumeCnt)
	{
		updateTimeLabel(m_stuMediaInfo.duration, m_stuMediaInfo.duration);
		updateTimeSliderPosition(m_stuMediaInfo.duration);
		std::thread t1([this]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			uninitAllSubModule();
			});
		t1.detach();
	}
}

void VideoPlayback::clearAudioDevice()
{
	if (m_ptrAudioOutput)
	{
		m_ptrAudioOutput->stop();
		delete m_ptrAudioOutput;
		m_ptrAudioOutput = nullptr;
	}
}

bool VideoPlayback::initOutputParameter(const AudioInfo& audioInfo)
{
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

bool VideoPlayback::startPlay()
{
	if (m_ptrAudioOutput)
	{
		m_ptr_AudioDevice = m_ptrAudioOutput->start();
	}
	return nullptr == m_ptr_AudioDevice ? false : true;
}

void VideoPlayback::inputPcmData(const QByteArray& pcmData)
{
	if (m_ptr_AudioDevice)
	{
		m_ptr_AudioDevice->write(pcmData);
	}
}

#if defined(BlackMagicEnabled)
void VideoPlayback::initSDIOutput()
{
	if (nullptr == m_ptrSelectedDeckLinkOutput)
	{
		return;
	}
	std::unique_lock<std::mutex> lck(m_mutex);
	// 初始化SDI输出
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
		// 处理错误
		return;
	}

	std::wstring deviceNameWStr = utils::BSTRToWString(deviceNameBSTR.m_str);
	std::string deviceNameStr = utils::WStringToString(deviceNameWStr);
	QString deviceName = QString::fromStdString(deviceNameStr);

	// bool bActive = isDeviceActive(deckLink);
	MyDeckLinkDevice device;
	device.deckLink = deckLink;
	device.m_strDisplayName = deviceName;
	device.m_bActive = true;
	m_mapDeviceNameIndex[device.m_strDisplayName] = device;

	ui.deckLinkComboBox->addItem(device.m_strDisplayName);
}
#endif //(WIN32)

#endif //BlackMagicEnabled