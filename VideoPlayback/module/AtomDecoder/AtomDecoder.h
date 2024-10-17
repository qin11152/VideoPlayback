#pragma once

#include "CommonDef.h"
#include "module/AtomDecoder/Buffer.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <atomic>
#include <mutex>
#include <memory>
#include <queue>
#include <thread>
#include <functional>
#include <condition_variable>

extern std::function<void(AVFrame *)> avframedel;
using avframe_ptr = std::unique_ptr<AVFrame, decltype(avframedel)>;

using PreviewCallback = std::function<void(const VideoCallbackInfo& videoInfo, int64_t currentTime)>;
using VideoOutputCallback = std::function<void(const VideoCallbackInfo& videoInfo)>;
using AudioCallback = std::function<void(std::vector<Buffer*>)>;

class AtomDecoder 
{
public:

	AtomDecoder();
	~AtomDecoder();

    int32_t initModule(const char* videoFileName, std::vector< std::pair<std::string, AudioInfo>> vecAudioFileNameAndInfo, const VideoInfo& outVideoInfo);

	void unInitModule();

	void readFrameFromFile();

	void decodeVideo();

    void decodeAudio();

	AVCodecContext *getVideoCodecContext() const { return videoCodecContext; }

	void initVideoCallBack(PreviewCallback preCallback, VideoOutputCallback videoOutputCallback);

    void initAudioCallback(AudioCallback audioCallback);

	void startDecoder();

private:
    AVFormatContext *formatContext{nullptr};
    AVCodecContext *videoCodecContext{nullptr};

    std::vector<std::pair<AVCodecContext*,int32_t>> m_vecAudioCodecContext;
    std::vector<AVFormatContext*> m_vecAudioFormatContext;
	std::vector<SwrContext*> m_vecSwrContext;
    std::vector<Buffer*> m_vecBuffer;
    int videoStreamIndex;
    SwsContext *swsContext{nullptr};
    //SwrContext *swrContext{nullptr};

    PreviewCallback m_previewCallback;
    AudioCallback m_AudioCallback;
    VideoOutputCallback m_videoOutputCallback;

    std::thread m_ReadThread;
    std::thread m_VideoDecoderThread;
    std::thread m_AudioDecoderThread;
    std::mutex m_PacketMutex;
	std::mutex m_AudioPacketMutex;
    std::condition_variable m_VideoCV;
    std::condition_variable m_AudioCV;
    std::condition_variable m_ReadCV;

    std::condition_variable m_PauseCV;

    std::queue<AVPacket> m_queueVideoFrame;
    std::queue<std::pair<AVPacket, uint32_t>> m_queueAudioFrame;

    std::atomic<bool> m_bSeekState{false};
    std::atomic<double_t> m_dSeekTime{0};

    bool m_bInitState{false};
    bool m_bRunningState{false};

    std::vector<std::pair<std::string, AudioInfo>> m_vecAudioInfo;
    //AudioInfo m_stuAudioInfo;
    VideoInfo m_stuVideoInfo;

    int64_t m_iStartTime{0};
    uint32_t m_uiReadThreadSleepTime{0};

    uint64_t m_uiVideoCurrentTime{0};
    uint64_t m_uiAudioCurrentTime{0};
};