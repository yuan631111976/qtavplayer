#ifndef AVCODEC_H
#define AVCODEC_H

#include <QObject>
#include <QAudioFormat>
#include <QWaitCondition>
#include <QBuffer>
#include <QDebug>
#include <QTime>
#include "AVDefine.h"
#include "AVThread.h"


#include "AVMediaCallback.h"

//32k
#define FFMPEG_AVIO_INBUFFER_SIZE 1024 * 32

extern "C"
{

    #ifndef INT64_C
    #define INT64_C
    #define UINT64_C
    #endif

    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>

    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
    #include <libavutil/opt.h>

    #include <libswresample/swresample.h>

    #include <libavutil/hwcontext.h>

    #ifdef LIBAVUTIL_VERSION_MAJOR
    #if (LIBAVUTIL_VERSION_MAJOR >= 56)
    #define ENABLE_HW
    #endif
    #endif
}

struct VideoFormat{
    float width;
    float height;
    float rotate;
    int format;
    int linesize[3];

    AVFrame *renderFrame;
    QMutex *renderFrameMutex;
};

class AVDecoder
{
public:
    AVDecoder();
    ~AVDecoder();

    typedef struct PacketQueue {
        AVPacketList *first_pkt, *last_pkt;
        int nb_packets;
        int size;
        int64_t time;
        QMutex *mutex;
        QWaitCondition *cond;
        bool isInit;
    } PacketQueue;

public:
    void setMediaCallback(AVMediaCallback *);
    void setFilename(const QString &source);
    void load();
    void seek(int ms);
    void setBufferSize(int size);
    void setMediaBufferMode(AVDefine::MediaBufferMode mode);
    void checkBuffer();//检查是否需要填充buffer
    bool hasVideo();
    bool hasAudio();
    /** 设置播放速率，最大为8，最小为1.0 / 8 */
    void setPlayRate(float);
    float getPlayRate();
    /** 渲染第一帧 */
    void renderFirstFrame();
    int getCurrentVideoTime();
    /** 渲染最后第一帧 */
    void renderNextFrame();
    /** 请求向音频buffer添加数据  */
    void requestAudioNextFrame(int);
    /** video是否播放完成 */
    bool isVideoPlayed();
public:
    void slotSeek(int ms);
    void slotSetBufferSize(int size);
    void slotSetMediaBufferMode(AVDefine::MediaBufferMode mode);
    void slotRenderFirstFrame();
    void slotRenderNextFrame();
    void slotRequestAudioNextFrame(int);
    void slotSetPlayRate(float);
public:
    void init();
    void release(bool isDeleted = false);
    void decodec();
    void setFilenameImpl(const QString &source);
private:
    void packet_queue_init(PacketQueue *q);
    int packet_queue_put(PacketQueue *q, AVPacket *pkt);
    int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
    void packet_queue_flush(PacketQueue *q);
    void packet_queue_destroy(PacketQueue *q);

    void statusChanged(AVDefine::MediaStatus);

private :
    int mAudioIndex;
    int mVideoIndex;
    int mSubTitleIndex;

    AVFormatContext *mFormatCtx;

    AVCodecContext *mAudioCodecCtx;
    AVCodecContext *mVideoCodecCtx;
    QMutex mVideoCodecCtxMutex;
    QMutex mVideoDecodecMutex;

    AVCodec *mAudioCodec;
    AVCodec *mVideoCodec;

    bool mIsOpenAudioCodec;
    bool mIsOpenVideoCodec;
    bool mHasSubtitle;

    AVPacket mPacket;
    AVFrame *mFrame, //视频帧缓冲1
            *mFrame1, //视频帧缓冲2
            *mAudioFrame,
            *mFrameYUV,
            *mHWFrame; //硬解BUFFER
    int mFrameIndex; //使用的
    uint8_t *mYUVBuffer;

    QString mFilename;

    int mFrameFinished;
    bool mIsReadFinish;
    bool mIsVideoLoadedCompleted; //视频是否加载完成
    bool mIsAudioLoadedCompleted; //音频是否加载完成
    bool mIsSeek; //是否进行了拖动处理
    bool mIsSeekd; //当所有元素拖动完成后，些属性为true
    bool mIsAudioSeeked; // 音频是否拖动完成
    bool mIsVideoSeeked; //视频是否拖动完成
    bool mIsSubtitleSeeked; //
    int mSeekTime; //拖动的时间
    bool mIsInit;

    int mIsAudioPlayed; // 音频是否播放完成
    int mIsVideoPlayed;  //视频是否播放完成
    int mIsSubtitlePlayed;

    /** 视频的旋转角度，由于手机录出来的视频带有旋转度数 */
    int mRotate;

    int mBufferSize;
    bool mIsVideoBuffered;
    bool mIsAudioBuffered;
    bool mIsSubtitleBuffered;
    AVDefine::MediaBufferMode mMediaBufferMode;

    AVDefine::MediaStatus mStatus;
    QByteArray mAudioBuffer;
    QMutex mAudioBufferMutex;
    uint8_t *mAudioDstData;
    PacketQueue audioq;
    PacketQueue videoq;
    PacketQueue subtitleq;

    VideoFormat vFormat;
    QMutex mRenderFrameMutex;
    QMutex mRenderFrameMutex2;
    bool mIsDestroy;
    int mVideoDecodedCount; //已解码的包的数量


    QMutex mAudioSwrCtxMutex;
    SwrContext* mAudioSwrCtx; //音频参数转换上下文
    QMutex mVideoSwsCtxMutex;
    struct SwsContext *mVideoSwsCtx; //视频参数转换上下文

    /** 媒体内存IO上下文 */
    AVIOContext *mAvioCtx;
    /** avio零时buffer */
    uint8_t *mAvioBuffer;
    /** 网速计时器  */
    QTime mTime;
    /** 网速计时器上一次的时间 */
    int mLastTime;
    /** 上一次下载的位置 */
    qint64 mLastPos;
    /** 播放速率 */
    float mPlayRate;
    /** 源生音频格式 */
    QAudioFormat mSourceAudioFormat;
    /** 己写入的音频字节数量 */
    qint64 mAlreadyWroteAudioSize;

    /** 是否支持硬解 */
    bool mIsSupportHw;
    /** 是否启用硬解 */
    bool mIsEnableHwDecode;

public :
    /** 任务处理线程 */
    AVThread mProcessThread;

    AVMediaCallback *mCallback;

    QMutex mMutex;

    /** 硬解上下文 */
    AVBufferRef *mHWDeviceCtx;
    /** 硬解格式 */
    enum AVPixelFormat mHWPixFormat;
};

class AVCodecTask : public Task{
public :
    ~AVCodecTask(){}
    enum AVCodecTaskCommand{
        AVCodecTaskCommand_Init,
        AVCodecTaskCommand_SetPlayRate,
        AVCodecTaskCommand_Seek,
        AVCodecTaskCommand_SetBufferSize,
        AVCodecTaskCommand_SetMediaBufferMode,
        AVCodecTaskCommand_Decodec ,
        AVCodecTaskCommand_SetFileName
    };
    AVCodecTask(AVDecoder *codec,AVCodecTaskCommand command,double param = 0,QString param2 = ""):
        mCodec(codec),command(command),param(param),param2(param2){}
protected :
    /** 现程实现 */
    virtual void run();
private :
    AVDecoder *mCodec;
    AVCodecTaskCommand command;
    double param;
    QString param2;
};

#endif // AVCODEC_H
