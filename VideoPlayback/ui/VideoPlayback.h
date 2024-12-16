#pragma once

#include <QtWidgets/QWidget>
#include "ui_VideoPlayback.h"
#include "module/AudioPlay/AudioPlay.h"
#include "module/VideoDecoder/AtomDecoder.h"
#include "module/VideoDecoder/VideoDecoder.h"
#include "module/VideoReader/VideoReader.h"
#include "module/VideoDecoder/VideoDecoderBase.h"
#include "module/decoderedDataHandler/PcmDatahandler.h"
#include "module/decoderedDataHandler/YuvDataHandler.h"
#include "module/decoderedDataHandler/PreviewAndPlay/AtomPreviewAndPlay.h"

#if defined(BlackMagicEnabled)
    #include "module/BlackMagic/DeckLinkDeviceDiscovery/DeckLinkDeviceDiscovery.h"
#endif (BlackMagicEnabled)

#include <map>
#include <mutex>

class HardDecoder;

class VideoPlayback : public QWidget
{
    Q_OBJECT

public:
    VideoPlayback(QWidget *parent = nullptr);
    ~VideoPlayback();

    bool initModule();

    //************************************
    // Method:    previewCallback
    // FullName:  VideoPlayback::previewCallback
    // Access:    public 
    // Returns:   void
    // Qualifier:
	// brief: ��Ƶ�Ļص���������Ƶ����
    // Parameter: std::shared_ptr<VideoCallbackInfo> videoInfo
    //************************************
    void previewCallback(std::shared_ptr<VideoCallbackInfo> videoInfo);

    //************************************
    // Method:    audioPlayCallBack
    // FullName:  VideoPlayback::audioPlayCallBack
    // Access:    public 
    // Returns:   void
    // Qualifier:
    // brief: ��Ƶ�Ļص���������Ƶ����
    // Parameter: std::shared_ptr<AudioCallbackInfo> audioInfo
    //************************************
    void audioPlayCallBack(std::shared_ptr<AudioCallbackInfo> audioInfo);

    void SDIOutputCallback(const VideoCallbackInfo& videoInfo);

    void atomAudioCallback(std::shared_ptr<AudioCallbackInfo> audioInfo);

signals:
    void signalYUVData(QByteArray data, const VideoInfo& videoInfo);

public slots:
    QString onSignalChooseFileClicked();

    void onSignalSliderValueChanged(double vlaue);

    void onSignalSelectedDeviceChanged(int index);

protected:
    void customEvent(QEvent* event) override;

private:
    bool initConnect();
    bool initAudioOutput();
    bool initAllSubModule();
    bool uninitAllSubModule();

    void updateTimeLabel(const int currentTime, const int totalTime);
	void updateTimeSliderPosition(int64_t currentTime);
    void setTimeSliderRange(int64_t totalTime);

    void onDecoderFinshed();
    void onConsumeFinished();

private:
    Ui::VideoPlaybackClass ui;

    VideoInfo m_stuVideoInfo;
    AudioInfo m_stuAudioInfo;

    std::shared_ptr<VideoReader> m_ptrVideoReader{ nullptr };
    std::vector<std::shared_ptr<VideoReader>> m_vecVideoReader;
	std::shared_ptr<VideoDecoderBase> m_ptrVideoDecoder{ nullptr };
	std::shared_ptr<PreviewAndPlay> m_ptrPreviewAndPlay{ nullptr };
	std::shared_ptr<AtomPreviewAndPlay> m_ptrAtomPreviewAndPlay{ nullptr };

    AudioPlay* m_ptrAudioPlay{ nullptr };
    QString m_strChooseFileName{ "" };
    MediaInfo m_stuMediaInfo;

	AVHWDeviceType m_eDeviceType{ AV_HWDEVICE_TYPE_NONE };

	std::vector<QString> m_vecChooseNameAtom;
    uint32_t m_uiAtomFrameCnt{ 0 };
    uint32_t m_uiAtomAudioSampleRate{ 0 };
    uint32_t m_uiAtomAudioSampleCntPerFrame{ 0 };

	std::mutex m_mutex;

	bool m_bSliderEnableConnect{ true };       //�Ƿ����������źŲ�
	bool m_bAtomFileValid{ false };

//////// �忨�������
private:
#if defined(BlackMagicEnabled)
	DeckLinkDeviceDiscovery* m_ptrDeckLinkDeviceDiscovery{ nullptr };
    std::map<QString, MyDeckLinkDevice> m_mapDeviceNameIndex;
    void initSDIOutput();

    #if defined(WIN32)
	    void deviceDiscovered(CComPtr<IDeckLink>& deckLink);
	    CComQIPtr<IDeckLinkOutput> m_ptrSelectedDeckLinkOutput{ nullptr };
    #elif defined(__linux__)
	    IDeckLinkOutput* m_ptrSelectedDeckLinkOutput{ nullptr };
    #endif (WIN32)

#endif BlackMagicEnabled
};
