#ifndef AVCODEC_H
#define AVCODEC_H

#include <QObject>
#include <QThread>
#include <QAudioFormat>
#include <QMutex>
#include <QWaitCondition>
#include <QBuffer>
#include <QFile>
#include <QDebug>
#include <QTime>
#include "U_YuvManager.h"

//32k
#define FFMPEG_AVIO_INBUFFER_SIZE 1024 * 32

extern "C"
{
#ifdef __cplusplus
 #define __STDC_CONSTANT_MACROS
 #ifdef _STDINT_H
  #undef _STDINT_H
 #endif
 # include <stdint.h>
#endif

    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>


    #include <libavfilter/avfiltergraph.h>
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
    #include <libavutil/opt.h>

    #include <libswresample/swresample.h>
}

struct VideoFormat{
    float width;
    float height;
    float rotate;
    int format;
    QMutex *mutex;
};

class AVCodec2 : public QObject
{
    Q_OBJECT
public:
    AVCodec2();
    ~AVCodec2();

    enum BufferMode{
        Time,//ms
        Packet
    };

    enum Status {
        UnknownStatus ,
        NoMedia ,
        Loading ,
        Loaded ,
        Seeking ,
        Seeked ,
        Stalled ,
        Buffering ,
        Buffered ,
        Played,
        EndOfMedia ,
        InvalidMedia
    };

    typedef struct PacketQueue {
        AVPacketList *first_pkt, *last_pkt;
        int nb_packets;
        int size;
        int64_t time;
        QMutex *mutex;
        QWaitCondition *cond;
    } PacketQueue;
    list<Yuv*> yuvs;
public:
    void setFilename(const QString &source);
    void load();
    void play();
    void seek(int ms);
    void setBufferSize(int size);
    void setBufferMode(BufferMode mode);
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
public slots:
    void print(){
//        if(mFormatCtx && mFormatCtx->pb){
//            qDebug() <<mFormatCtx->pb->bytes_read << "writeout_count : " <<
//                       mFormatCtx->pb->pos;
//        }
    }

signals :
    void updateVideoFrame(const char*,VideoFormat*);

    void updateAudioFormat(QAudioFormat);
    void updateAudioFrame(const QByteArray &buffer);

    void hasAudioChanged();
    void hasVideoChanged();
    /** 视频总时间变化信息 */
    void durationChanged(int duration);
    /** 可以渲染第一帧 */
    void canRenderFirstFrame();
    void statusChanged(AVCodec2::Status);
    /** 更新下载速率bytes len */
    void updateInternetSpeed(int speed);
signals :
    void sigLoad();
    void sigPlay();
    void sigSeek(int ms);
    void sigSetBufferSize(int size);
    void sigSetBufferMode(BufferMode mode);
    void sigCheckBuffer();
    void sigRenderFirstFrame();
    void sigRenderNextFrame();
    void sigRequestAudioNextFrame(int);
    void sigSetPlayRate(float);
private slots :
    void slotLoad();
    void slotSeek(int ms);
    void slotSetBufferSize(int size);
    void slotSetBufferMode(BufferMode mode);
    void slotCheckBuffer();
    void slotRenderFirstFrame();
    void slotRenderNextFrame();
    void slotRequestAudioNextFrame(int);
    void slotSetPlayRate(float);
private slots:
    void init();
    void release();
    void decodec();
private:
    int getWidth();
    int getHeight();

    void packet_queue_init(PacketQueue *q);
    int packet_queue_put(PacketQueue *q, AVPacket *pkt);
    int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
    void packet_queue_flush(PacketQueue *q);
    void packet_queue_destroy(PacketQueue *q);

    void sigStatusChanged(AVCodec2::Status);
private :
    int mAudioIndex;
    int mVideoIndex;
    int mSubTitleIndex;

    AVFormatContext *mFormatCtx;

    AVCodecContext *mAudioCodecCtx;
    AVCodecContext *mVideoCodecCtx;

    AVCodec *mAudioCodec;
    AVCodec *mVideoCodec;

    bool mIsOpenAudioCodec;
    bool mIsOpenVideoCodec;
    bool mHasSubtitle;

    AVPacket mPacket;
    AVFrame *mFrame,
            *mAudioFrame,
            *mFrameYUV;
    uint8_t *mYUVBuffer;
    char *mYUVTransferBuffer;

    QString mFilename;

    int mFrameFinished;
    bool mIsReadFinish;
    bool mIsSeek; //是否进行了拖动处理
    bool mIsSeekd; //当所有元素拖动完成后，些属性为true
    bool mIsAudioSeeked; // 音频是否拖动完成
    bool mIsVideoSeeked; //视频是否拖动完成
    bool mIsSubtitleSeeked; //
    int mSeekTime; //拖动的时间

    int mIsAudioPlayed; // 音频是否播放完成
    int mIsVideoPlayed;  //视频是否播放完成
    int mIsSubtitlePlayed;

    QThread mThread;
    /** 视频的旋转角度，由于手机录出来的视频带有旋转度数 */
    int mRotate;

    int mBufferSize;
    bool mIsVideoBuffered;
    bool mIsAudioBuffered;
    bool mIsSubtitleBuffered;
    BufferMode mBufferMode;

    Status mStatus;
    QByteArray mAudioBuffer;
    uint8_t *mAudioDstData;
    PacketQueue audioq;
    PacketQueue videoq;
    PacketQueue subtitleq;

    VideoFormat vFormat;
    QMutex m_mutex;
    QMutex mVideoDecodecMutex;
    bool m_isDestroy;

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
};

#endif // AVCODEC_H
