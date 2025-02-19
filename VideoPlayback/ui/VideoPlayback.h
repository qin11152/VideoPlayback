#pragma once

#include <QtWidgets/QWidget>
#include "ui_VideoPlayback.h"
#include "module/VideoDecoder/AtomDecoder.h"
#include "module/VideoDecoder/VideoDecoder.h"
//#include "module/VideoReader/VideoReader.h"
#include "module/demux/demuxer.h"
#include "module/VideoDecoder/VideoDecoderBase.h"
#include "module/decoderedDataHandler/PcmDatahandler.h"
#include "module/decoderedDataHandler/YuvDataHandler.h"
#include "module/decoderedDataHandler/PreviewAndPlay/PreviewAndPlay.h"
#include "module/decoderedDataHandler/PreviewAndPlay/AtomPreviewAndPlay.h"

#if defined(BlackMagicEnabled)
    #include "module/BlackMagic/DeckLinkDeviceDiscovery/DeckLinkDeviceDiscovery.h"
#endif //BlackMagicEnabled

#include <QIODevice>
#include <QAudioOutput>

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
	// brief: 视频的回调，进行视频播放
    // Parameter: std::shared_ptr<DecodedImageInfo> videoInfo
    //************************************
    void previewCallback(std::shared_ptr<DecodedImageInfo> videoInfo);

    //************************************
    // Method:    audioPlayCallBack
    // FullName:  VideoPlayback::audioPlayCallBack
    // Access:    public 
    // Returns:   void
    // Qualifier:
    // brief: 音频的回调，进行音频播放
    // Parameter: std::shared_ptr<AudioCallbackInfo> audioInfo
    //************************************
    void audioPlayCallBack(std::shared_ptr<AudioCallbackInfo> audioInfo);

    void SDIOutputCallback(const DecodedImageInfo& videoInfo);

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
	void updateTimeSliderPosition(double currentTime);
    void setTimeSliderRange(int64_t totalTime);

    void onDecoderFinshed();
    void onConsumeFinished();

    void clearAudioDevice();
	bool initOutputParameter(const AudioInfo& audioInfo);
    bool startPlay();
	void inputPcmData(const QByteArray& pcmData);

private:
    Ui::VideoPlaybackClass ui;

    VideoInfo m_stuVideoInfo;
    AudioInfo m_stuAudioInfo;

    std::shared_ptr<demuxer> m_ptrDemuxer{ nullptr };
    std::vector<std::shared_ptr<demuxer>> m_vecDemuxer;
	std::shared_ptr<VideoDecoderBase> m_ptrVideoDecoder{ nullptr };
	std::shared_ptr<PreviewAndPlay> m_ptrPreviewAndPlay{ nullptr };
	std::shared_ptr<AtomPreviewAndPlay> m_ptrAtomPreviewAndPlay{ nullptr };

    //AudioPlay* m_ptrAudioPlay{ nullptr };
    QString m_strChooseFileName{ "" };
    MediaInfo m_stuMediaInfo;

	AVHWDeviceType m_eDeviceType{ AV_HWDEVICE_TYPE_NONE };

	std::vector<QString> m_vecChooseNameAtom;
    uint32_t m_uiAtomFrameCnt{ 0 };
    uint32_t m_uiAtomAudioSampleRate{ 0 };
    uint32_t m_uiAtomAudioSampleCntPerFrame{ 0 };

	std::mutex m_mutex;

	bool m_bSliderEnableConnect{ true };       //是否允许连接信号槽
	bool m_bAtomFileValid{ false };

	QAudioOutput* m_ptrAudioOutput{ nullptr };
	QIODevice* m_ptr_AudioDevice{ nullptr };

    uint32_t m_uiConsumeCnt{ 0 };
    uint32_t m_uiConsumeCallbackCnt{ 0 };

//////// 板卡相关内容
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
    #endif //(WIN32)

#endif //BlackMagicEnabled
};
