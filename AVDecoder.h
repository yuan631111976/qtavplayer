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

#define SAMPLE_ARRAY_SIZE (8 * 65536)

extern "C"
{

    #ifndef INT64_C
    #define INT64_C
    #define UINT64_C
    #endif

    #include <libavcodec/avcodec.h>
    #include <libavcodec/avfft.h>


    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>

    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
    #include <libavfilter/avfilter.h>

    #include <libavutil/opt.h>

    #include <libswresample/swresample.h>

    #include <libavutil/hwcontext.h>

    #ifdef LIBAVUTIL_VERSION_MAJOR
    #if (LIBAVUTIL_VERSION_MAJOR >= 56)
    #define SUPPORT_HW
    #endif
    #endif
}

struct VideoFormat{
    float width;
    float height;
    float rotate;
    int format;

    AVFrame *renderFrame;
    QMutex *renderFrameMutex;
};

class RenderItem{
public :
    RenderItem()
        : pts(AV_NOPTS_VALUE)
        , valid (false)
        , isRendered(false)
        , isHWDecodec(false)
    {
        frame = av_frame_alloc();
    }

    void release(){
        if(!isHWDecodec && !isConverted){
            if(frame != NULL){
                av_frame_unref(frame);
            }
        }
        pts = AV_NOPTS_VALUE;
        valid = false;
        isRendered = false;
    }

    void clear(){
        if(frame != NULL){
            av_frame_unref(frame);
        }
        pts = AV_NOPTS_VALUE;
        valid = false;
        isRendered = false;
    }

    ~RenderItem(){
        if(frame != NULL){
            av_frame_unref(frame);
            av_frame_free(&frame);
        }
        pts = AV_NOPTS_VALUE;
        valid = false;
        isRendered = false;
    }

    AVFrame * frame; //帧
    qint64 pts; //播放时间
    bool valid; //有效的
    bool isRendered; //是否已经渲染
    bool isHWDecodec; //是否是硬解
    bool isConverted; //是否是转换过的帧
    QMutex mutex;
};

class PacketQueue{
public :
    PacketQueue(){init();}

    QList<AVPacket *> packets;
    AVRational time_base;

private :
    QMutex mutex;
public :

    void setTimeBase(AVRational &timebase){
        this->time_base.den = timebase.den;
        this->time_base.num = timebase.num;
    }

    void init(){
        release();
    }

    void put(AVPacket *pkt){
        if(pkt == NULL)
            return;
        mutex.lock();
        packets.push_back(pkt);
        mutex.unlock();
    }

    AVPacket *get(){
        AVPacket *pkt = NULL;
        mutex.lock();
        if(packets.size() > 0){
            pkt = packets.front();
            packets.pop_front();
        }
        mutex.unlock();
        return pkt;
    }

    void removeToTime(int time){
        mutex.lock();
        QList<AVPacket *>::iterator begin = packets.begin();
        QList<AVPacket *>::iterator end = packets.end();
        while(begin != end){
            AVPacket *pkt = *begin;
            if(pkt != NULL){
                if(av_q2d(time_base) * pkt->pts * 1000 >= time || packets.size() == 1){
                    break;
                }
                av_packet_unref(pkt);
                av_freep(pkt);
            }
            packets.pop_front();
            begin = packets.begin();
        }
        mutex.unlock();
    }

    int diffTime(){
        int time = 0;
        mutex.lock();
        if(packets.size() > 1){
            int start = av_q2d(time_base) * packets.front()->pts * 1000;
            int end = av_q2d(time_base) * packets.back()->pts * 1000;
            time = end - start;
        }
        mutex.unlock();
        return time;
    }

    int startTime(){
        int time = -1;
        mutex.lock();
        if(packets.size() > 0){
            time = av_q2d(time_base) * packets.front()->pts * 1000;
        }
        mutex.unlock();
        return time;
    }

    int size(){
        mutex.lock();
        int len = packets.size();
        mutex.unlock();
        return len;
    }

    void release(){
        mutex.lock();
        QList<AVPacket *>::iterator begin = packets.begin();
        QList<AVPacket *>::iterator end = packets.end();
        while(begin != end){
            AVPacket *pkt = *begin;
            if(pkt != NULL){
                av_packet_unref(pkt);
                av_freep(pkt);
            }
            packets.pop_front();
            begin = packets.begin();
        }
        mutex.unlock();
    }
};

class AVDecoder : public QObject
{
public:
    AVDecoder();
    ~AVDecoder();

public:
    void setMediaCallback(AVMediaCallback *);
    void setFilename(const QString &source);
    void load();
    void seek(int ms);
    void checkBuffer();//检查是否需要填充buffer
    bool hasVideo();
    bool hasAudio();
    /** 设置播放速率，最大为8，最小为1.0 / 8 */
    void setPlayRate(int);
    int getPlayRate();
    /** 渲染第一帧 */
    void renderFirstFrame();
    int requestRenderNextFrame();
    /** 渲染最后第一帧 */
    void checkRenderList();
    /** 请求向音频buffer添加数据  */
    void requestAudioNextFrame(int);
    /** video是否播放完成 */
    bool isVideoPlayed();

    qint64 nextTime();

    bool getAccompany()const;
    void setAccompany(bool flag);

    /** 显示指定位置的帧 */
    void showFrameByPosition(int time);

    /** 设置音频通道 */
    bool setAudioChannel(AVDefine::AVChannelLayout);
    int getAudioChannel()const;

    float getRealPlayRate()const {return mRealPlayRate;}

    ///获取播放路径
    QString getPlayPath() const {return mFilename;}

    /** 丢弃帧到指定的时间*/
    void throwAwaysFrameToTime(int time);

    //获取ffmpeg的误信息
    QString getFFMpegError(int no){
        char msg[512];
        av_make_error_string(msg,512,no);
        return QString(msg);
    }

    QVector<int> getsupportDecodecModeList();

    ///是否是直播
    bool isLiving() const {return mIsLiving;}

    /** 是否是预览 */
    GENERATE_GET_SET_PROPERTY_CHANGED_IMPL_SET(preview,bool)
    GENERATE_GET_SET_PROPERTY_CHANGED_IMPL_SET(minimumBufferSize,int)
    GENERATE_GET_SET_PROPERTY_CHANGED_IMPL_SET(maximumBufferSize,int)
    GENERATE_GET_SET_PROPERTY_CHANGED_NO_IMPL(decodecMode,int)
public:
    void slotSeek(int ms);
    void slotSetBufferSize(int size);
    void slotSetMediaBufferMode(AVDefine::AVMediaBufferMode mode);
    void slotRenderFirstFrame();
    void slotRenderNextFrame();
    void slotRequestAudioNextFrame(int);
    void slotSetPlayRate(int);
public:
    void init();
    void release(bool isDeleted = false);
    void decodec();
    void showFrameByPositionImpl(int time);
    void setFilenameImpl(const QString &source);
    void stop();

    void releseCurrentRenderItem();
    void resetVideoCodecContext();
    void slotSetDecoecMode(int value);
private:
    void statusChanged(AVDefine::AVMediaStatus);


    void initRenderList();
    int getRenderListSize();
    void clearRenderList(bool isDelete = false);
    RenderItem *getInvalidRenderItem();
    void changeRenderItemSize(int width,int height,AVPixelFormat format);

    bool initAudioFilter();
    void releaseAudioFilter();

    //计算频谱
    void calcSpectrum(uint16_t *pcm);
    //初使化视频解码器
    void initVideoContext();

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


    AVFrame *mAudioFrame,
            *reciveFrame,
            *mHWFrame; //硬解BUFFER
    QVector<RenderItem *> mRenderList; //渲染队列
    int maxRenderListSize; //渲染队列最大数量
    QMutex mRenderListMutex; //队列操作锁
    int mRenderListCount;//队列数量
    RenderItem *mLastRenderItem; //上一次的渲染对象


    int mFrameIndex; //使用的

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
    bool mIsNeedCallRenderFirstFrame;

    bool mIsAudioPlayed; // 音频是否播放完成
    bool mIsVideoPlayed;  //视频是否播放完成  
    bool mIsSubtitlePlayed;

    /** 视频的旋转角度，由于手机录出来的视频带有旋转度数 */
    int mRotate;

    bool mIsVideoBuffered;
    bool mIsAudioBuffered;
    bool mIsSubtitleBuffered;

    AVDefine::AVMediaStatus mStatus;
    QByteArray mAudioBuffer;
    QMutex mAudioBufferMutex;

    PacketQueue audioq;
    PacketQueue videoq;
    PacketQueue subtitleq;

    VideoFormat vFormat;
    QMutex mRenderFrameMutex;
    QMutex mRenderFrameMutex2;
    bool mIsDestroy;

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
    int mPlayRate;
    /** 实际的播放比率 */
    float mRealPlayRate;
    /** 源采样率 */
    int mSourceSampleRate;
    /** 源生音频格式 */
    QAudioFormat mSourceAudioFormat;
    /** 己写入的音频字节数量 */
    qint64 mAlreadyWroteAudioSize;

    /** 是否使用硬解 */
    bool mUseHw;
    /** 是否是伴唱 */
    bool mIsAccompany;
    /// 是否是直播
    bool mIsLiving;
    ///声音滤镜是否初使化成功
    bool mIsAudioFilterInited;


    AVFilterContext *mAudioBufferSinkCtx;
    AVFilterContext *mAudioBufferSrcCtx;
    AVFilterGraph *mAudioFilterGraph;
    AVFrame *mAudioFilterFrame;
    QMutex mAudioFilterMutex;

    AVFilterInOut *mAudioOutputs;
    AVFilterInOut *mAudioInputs;

    AVDefine::AVChannelLayout mAudioChannelLayout;
    uint64_t mOutChannelLayout;


#ifdef SUPPORT_HW
    QList<AVCodecHWConfig *> mHWConfigList;
    QMutex mHWConfigListMutex;
#endif
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
        AVCodecTaskCommand_Decodec ,
        AVCodecTaskCommand_SetFileName ,
        AVCodecTaskCommand_DecodeToRender,
        AVCodecTaskCommand_SetDecodecMode,
        AVCodecTaskCommand_ShowFrameByPosition,
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
