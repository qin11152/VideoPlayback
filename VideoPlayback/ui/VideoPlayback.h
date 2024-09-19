#pragma once

#include <QtWidgets/QWidget>
#include "ui_VideoPlayback.h"
#include "module/AudioPlay/AudioPlay.h"
#include "module/VideoDecoder/VideoDecoder.h"
#include "module/BlackMagic/DeckLinkDeviceDiscovery/DeckLinkDeviceDiscovery.h"

#include <map>

class VideoPlayback : public QWidget
{
    Q_OBJECT

public:
    VideoPlayback(QWidget *parent = nullptr);
    ~VideoPlayback();

    bool initModule();

    void previewCallback(const VideoCallbackInfo& videoInfo, int64_t currentTime);

    void AudioPlayCallBack(uint8_t** audioData, uint32_t channelSampleNumber);

signals:
    void signalYUVData(QByteArray data, const VideoInfo& videoInfo);

public slots:
    QString onSignalChooseFileClicked();

    void onSignalSliderValueChanged(double vlaue);

protected:
    void customEvent(QEvent* event) override;

private:
    bool initConnect();
    bool initAudioOutput();
    bool initDecoder();

    void updateTimeLabel(const int currentTime, const int totalTime);
	void updateTimeSliderPosition(int64_t currentTime);
    void setTimeSliderRange(int64_t totalTime);

private:
    Ui::VideoPlaybackClass ui;

    VideoDecoder* m_ptrVideoDecoder{ nullptr };
    AudioPlay* m_ptrAudioPlay{ nullptr };
    QString m_strChooseFileName{ "" };
    MediaInfo m_stuMediaInfo;
    DeckLinkDeviceDiscovery* m_ptrDeckLinkDeviceDiscovery{nullptr};

	bool m_bSliderEnableConnect{ true };       //是否允许连接信号槽
    std::map<QString, MyDeckLinkDevice> m_mapDeviceNameIndex;
};
