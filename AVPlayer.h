#ifndef AVPLAYER_H
#define AVPLAYER_H


#include <QtQml/QQmlParserStatus>
#include <QAudioOutput>
#include <QBuffer>
#include "AVDecoder.h"
#include "AVDefine.h"
#include "AVThread.h"
#include "AVMediaCallback.h"

class AVTimer;
class AVPlayer : public QObject , public AVMediaCallback ,public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool autoLoad READ autoLoad WRITE setAutoLoad NOTIFY autoLoadChanged)
    Q_PROPERTY(bool autoPlay READ autoPlay WRITE setAutoPlay NOTIFY autoPlayChanged)
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY hasAudioChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool renderFirstFrame READ renderFirstFrame WRITE setRenderFirstFrame)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(int pos READ pos WRITE seek NOTIFY positionChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(int bufferSize READ bufferSize WRITE setBufferSize)
    Q_PROPERTY(int bufferMode READ getMediaBufferMode WRITE setMediaBufferMode)

    GENERATE_QML_NOSET_PROPERTY(sourceWidth,int) //原视屏的宽度
    GENERATE_QML_NOSET_PROPERTY(sourceHeight,int)//原视屏的高度
    GENERATE_QML_PROPERTY(preview,bool)//是否是预览
    GENERATE_QML_PROPERTY(playSpeedRate,int)//播放速率，参数为AVDefine.PlaySpeedRate
    GENERATE_QML_PROPERTY(accompany,bool)//是否启用伴唱 //true : 伴唱 | false : 原唱 ，默认为false
    GENERATE_QML_PROPERTY(channelLayout,int)//AVDefine.AVChannelLayout
    GENERATE_QML_PROPERTY(kdMode,int)//AVDefine.AVKDMode



    GENERATE_GET_PROPERTY_CHANGED(sourceWidth,int)
    GENERATE_GET_PROPERTY_CHANGED(sourceHeight,int)
    GENERATE_GET_SET_PROPERTY_CHANGED_IMPL_SET(preview,bool)
    GENERATE_GET_SET_PROPERTY_CHANGED_NO_IMPL(playSpeedRate,int)
    GENERATE_GET_SET_PROPERTY_CHANGED_NO_IMPL(accompany,bool)
    GENERATE_GET_SET_PROPERTY_CHANGED_NO_IMPL(channelLayout,int)
    GENERATE_GET_SET_PROPERTY_CHANGED_NO_IMPL(kdMode,int)

public:
    AVPlayer();
    ~AVPlayer();

    QString source() const;
    void setSource(const QString&);

    bool autoLoad() const;
    void setAutoLoad(bool);

    bool autoPlay() const;
    void setAutoPlay(bool);

    bool hasAudio() const;
    bool hasVideo() const;
    int status() const;

    bool renderFirstFrame() const;
    void setRenderFirstFrame(bool);

    int bufferSize() const;
    void setBufferSize(int size);

    int volume() const;
    void setVolume(int vol);

    void slotSetPlayRate(int);

    int pos() const;
    int duration() const;

    int getMediaBufferMode() const;
    void setMediaBufferMode(int mode);


    VideoFormat *getRenderData();
    AVDefine::AVPlayState getPlaybackState();

    //QQmlParserStatus
    virtual void classBegin();
    virtual void componentComplete();
    //////
public slots:
    void play();
    void pause();
    void stop();
    void restart();
    void seek(int time);
    void showFrameByPosition(int time); //显示指定位置的帧
public :
    void seekImpl(int time);
//avmediacallback实现
public :
    void mediaUpdateAudioFormat(const QAudioFormat&);
    void mediaUpdateAudioFrame(const QByteArray &);
    void mediaUpdateVideoFrame(void*);
    void mediaDurationChanged(int);
    void mediaStatusChanged(AVDefine::AVMediaStatus);
    void mediaHasAudioChanged();
    void mediaHasVideoChanged();
    void mediaCanRenderFirstFrame();

signals :
    void durationChanged();
    void positionChanged();
    void sourceChanged();
    void autoLoadChanged();
    void autoPlayChanged();
    void hasAudioChanged();
    void hasVideoChanged();
    void statusChanged();
    void volumeChanged();
    void playStatusChanged();
    void updateVideoFrame(VideoFormat*);
    void playCompleted(); //播放完成

    //更新音频频谱
    void updateSpectrum();

public :
    void requestRender();
    void requestAudioData();
private :
    void wakeupPlayer();

    void setIsPaused(bool);
    bool getIsPaused();

    void setIsPlaying(bool);
    bool getIsPlaying();
private:
    AVDecoder *mDecoder;
    AVDefine::AVMediaStatus mStatus;
    AVDefine::AVSynchMode mSynchMode;

    bool mAutoLoad;
    bool mAutoPlay;
    bool mRrnderFirstFrame;

    int mBufferSize;
    float mVolume;
    int mDuration;
    int mPos;
    int mSeekTime;
    bool mIsSeeked;//是否拖动完成
    bool mIsAudioWaiting;
    QMutex mIsSeekedMutex;

    QString mSource;

    /** 组件是否加载完成 */
    bool mComplete;
    /** 视频播放控制线程 */
    AVThread mThread;
    AVThread mThread2;
    AVTimer *mAudioTimer;


    QMutex mAudioMutex;
    QAudioOutput* mAudio;
    QIODevice *mAudioBuffer;
    QMutex mAudioBufferMutex;

    int mLastTime;
    bool mIsDestroy;
    bool mIsPaused;
    bool mIsPlaying;
    bool mIsSetPlayRate;
    bool mIsSetPlayRateBeforeIsPaused;
    bool mIsClickedPlay;
    QMutex mStatusMutex;

    QWaitCondition mCondition;
    QMutex mMutex;

    /** 播放状态 */
    AVDefine::AVPlayState mPlaybackState;
    VideoFormat *mRenderData;
};

class AVPlayerTask : public Task{
public :
    ~AVPlayerTask(){}
    enum AVPlayerTaskCommand{
        AVPlayerTaskCommand_Render,
        AVPlayerTaskCommand_Seek ,
        AVPlayerTaskCommand_SetPlayRate
    };
    AVPlayerTask(AVPlayer *player,AVPlayerTaskCommand command,double param = 0):
        mPlayer(player),command(command),param(param){this->type = (int)command;}
protected :
    /** 线程实现 */
    virtual void run();
private :
    AVPlayer *mPlayer;
    AVPlayerTaskCommand command;
    double param;
};

class AVTimer : public QThread{

public :
    AVTimer(AVPlayer *player) : mIsRunning(true),mPlayer(player){
        start();
    }
    ~AVTimer(){
        mPlayer = NULL;
        stop();
        quit();
        wait(100);
        terminate();
    }
    virtual void run(){
        while(mIsRunning){
            if(mPlayer != NULL)
                mPlayer->requestAudioData();
            mMutex.lock();
            mCondition.wait(&mMutex,20);
            mMutex.unlock();
        }
    }

    void begin(){
        if(mPlayer != NULL && mPlayer->hasAudio()){
            mIsRunning = true;
            start();
        }
    }

    void stop(){
        mIsRunning = false;
        mCondition.wakeAll();
    }

private :
    bool mIsRunning;
    QWaitCondition mCondition;
    QMutex mMutex;
    AVPlayer *mPlayer;
};

#endif // AVPLAYER_H
