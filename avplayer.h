#ifndef AVPLAYER_H
#define AVPLAYER_H


#include <QtQml/QQmlParserStatus>
#include <QAudioOutput>
#include <QBuffer>
#include "AVMediaPlayer.h"
#include "AVDecoder.h"
#include "AVDefine.h"
#include "AVThread.h"
#include "AVMediaCallback.h"

class AVTimer;
class AVPlayer : public QObject , public AVMediaCallback , public AVMediaPlayer ,public QQmlParserStatus
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
    Q_PROPERTY(float playRate READ playRate WRITE setPlayRate NOTIFY playRateChanged)
    Q_PROPERTY(int pos READ pos WRITE seek NOTIFY positionChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(int bufferSize READ bufferSize WRITE setBufferSize)
    Q_PROPERTY(int bufferMode READ getMediaBufferMode WRITE setMediaBufferMode)
    Q_PROPERTY(bool accompany READ getAccompany WRITE setAccompany NOTIFY accompanyChanged) //true : 伴唱 | false : 原唱 ，默认为false
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

    float playRate() const;
    void setPlayRate(float);
    void slotSetPlayRate(float);

    int pos() const;
    int duration() const;

    int getMediaBufferMode() const;
    void setMediaBufferMode(int mode);

    bool getAccompany()const;
    void setAccompany(bool flag);

    VideoFormat *getRenderData();

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
public :
    void seekImpl(int time);
//avmediacallback实现
public :
    void mediaUpdateAudioFormat(const QAudioFormat&);
    void mediaUpdateAudioFrame(const QByteArray &);
    void mediaUpdateVideoFrame(void*);
    void mediaDurationChanged(int);
    void mediaStatusChanged(AVDefine::MediaStatus);
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
    void playRateChanged();
    void playStatusChanged();
    void updateVideoFrame(VideoFormat*);
    void playCompleted(); //播放完成
    void accompanyChanged();

public :
    void requestRender();
    void requestAudioData();
private :
    void wakeupPlayer();

//    bool mIsPaused;
//    bool mIsPlaying;
    void setIsPaused(bool);
    bool getIsPaused();

    void setIsPlaying(bool);
    bool getIsPlaying();
//avmediaplayer 实现
public slots:
    /** 设置文件路径 */
    void setFileName(const char *path);
    /** 获取当前音量 */
    int getVolume();
    /** 设置播放速率(0.125 - 8)
    *   0.125 以1/8的速度播放(慢速播放)
    *   8 以8/1的速度播放(快速播放)
    *   慢速播放公式(rate = 1.0 / 倍数(最大为8))
    *   快速播放公式(rate = 1.0 * 倍数(最大为8))
    */
    void setPlaybackRate(float rate);
    /** 获取当前播放速率 */
    float getPlaybackRate();
    /** 获取当前视频的总时长 */
    int getDuration();
    /** 获取当前视频的播放位置 */
    int getPosition();
    /** 获取当前媒体状态 */
    int getCurrentMediaStatus();
    /** 获取当前播放状态 */
    int getPlaybackState();
    /** 设置播放回调接口 */
    void setPlayerCallback(AVPlayerCallback *);
private:
    AVDecoder *mDecoder;
    AVDefine::MediaStatus mStatus;
    AVDefine::SynchMode mSynchMode;

    bool mAutoLoad;
    bool mAutoPlay;
    bool mRrnderFirstFrame;
    AVDefine::MediaBufferMode mMediaBufferMode;
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

    AVPlayerCallback *mPlayerCallback;
    /** 播放状态 */
    AVDefine::PlaybackState mPlaybackState;
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
