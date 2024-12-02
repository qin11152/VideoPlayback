#pragma once

#include <QtWidgets/QWidget>
#include "ui_VideoPlayback.h"
#include "module/AudioPlay/AudioPlay.h"
#include "module/AtomDecoder/AtomDecoder.h"
#include "module/VideoDecoder/VideoDecoder.h"
#include "module/BlackMagic/DeckLinkDeviceDiscovery/DeckLinkDeviceDiscovery.h"

#include <map>
#include <mutex>

class VideoPlayback : public QWidget
{
    Q_OBJECT

public:
    VideoPlayback(QWidget *parent = nullptr);
    ~VideoPlayback();

    bool initModule();

    void previewCallback(std::shared_ptr<VideoCallbackInfo> videoInfo, int64_t currentTime);

    void AudioCallback(std::vector<Buffer*> audioBuffer);

    void SDIOutputCallback(const VideoCallbackInfo& videoInfo);

    void AudioPlayCallBack(uint8_t* audioData, uint32_t channelSampleNumber);

    void clearDecoder();

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
    bool initDecoder();

    void updateTimeLabel(const int currentTime, const int totalTime);
	void updateTimeSliderPosition(int64_t currentTime);
    void setTimeSliderRange(int64_t totalTime);

    void initSDIOutput();

#if defined(WIN32)
    void deviceDiscovered(CComPtr<IDeckLink>& deckLink);
#endif

private:
    Ui::VideoPlaybackClass ui;

    std::shared_ptr<VideoDecoder> m_ptrVideoDecoder{ nullptr };
    AtomDecoder* m_ptrAtomDecoder{ nullptr };
    AudioPlay* m_ptrAudioPlay{ nullptr };
    QString m_strChooseFileName{ "" };
    MediaInfo m_stuMediaInfo;

	std::vector<QString> m_vecChooseNameAtom;
    uint32_t m_uiAtomFrameCnt{ 0 };
    uint32_t m_uiAtomAudioSampleRate{ 0 };
    uint32_t m_uiAtomAudioSampleCntPerFrame{ 0 };

    DeckLinkDeviceDiscovery* m_ptrDeckLinkDeviceDiscovery{nullptr};
#if defined(WIN32)
	CComQIPtr<IDeckLinkOutput> m_ptrSelectedDeckLinkOutput{ nullptr };
#elif defined(__linux__)
    IDeckLinkOutput* m_ptrSelectedDeckLinkOutput{ nullptr };
#endif
    std::mutex m_mutex;

	bool m_bSliderEnableConnect{ true };       //是否允许连接信号槽
    bool m_bAtomFileValid{ false };
    std::map<QString, MyDeckLinkDevice> m_mapDeviceNameIndex;
};
