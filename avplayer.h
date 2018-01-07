#ifndef AVPLAYER_H
#define AVPLAYER_H

#include <QtQml/QQmlParserStatus>
#include <QAudioOutput>
#include <QBuffer>
#include <QObject>
#include <QEvent>
#include <QTimer>
#include "avcodec.h"

#define EVENT_PLAYER_DRAW 1001

class AVPlayerThread : public QThread{
    Q_OBJECT
protected:
    bool event(QEvent *event){
        if(event->type() == EVENT_PLAYER_DRAW){
            emit onPlayerEvent();
            return true;
        }
        return QThread::event(event);
    }
signals:
    void onPlayerEvent();
};

class AVPlayer : public QObject ,public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_ENUMS(Loop)
    Q_ENUMS(PlaybackState)
    Q_ENUMS(Status)
    Q_ENUMS(Error)
    Q_ENUMS(ChannelLayout)
    Q_ENUMS(AVPlayer::BufferMode)

    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool autoLoad READ autoLoad WRITE setAutoLoad NOTIFY autoLoadChanged)
    Q_PROPERTY(bool autoPlay READ autoPlay WRITE setAutoPlay NOTIFY autoPlayChanged)
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY hasAudioChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)
    Q_PROPERTY(bool status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool renderFirstFrame READ renderFirstFrame WRITE setRenderFirstFrame)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(float playRate READ playRate WRITE setPlayRate NOTIFY playRateChanged)
    Q_PROPERTY(int pos READ pos WRITE seek)
    Q_PROPERTY(int duration READ duration)
    Q_PROPERTY(int bufferSize READ bufferSize WRITE setBufferSize)
    Q_PROPERTY(AVPlayer::BufferMode bufferMode READ bufferMode WRITE setBufferMode)
public:

    enum Loop { Infinite = -1 };

    enum PlaybackState {
        StoppedState,
        PlayingState,
        PausedState
    };
    enum Status {
        UnknownStatus = AVCodec2::Status::UnknownStatus,
        NoMedia = AVCodec2::Status::NoMedia ,
        Loading = AVCodec2::Status::Loading ,
        Loaded = AVCodec2::Status::Loaded ,
        Stalled = AVCodec2::Status::Stalled ,
        Buffering = AVCodec2::Status::Buffering ,
        Buffered = AVCodec2::Status::Buffered ,
        EndOfMedia = AVCodec2::Status::EndOfMedia ,
        InvalidMedia = AVCodec2::Status::InvalidMedia
    };
    enum Error {
        NoError,
        ResourceError,
        FormatError,
        NetworkError,
        AccessDenied,
        ServiceMissing
    };

    enum ChannelLayout {
        ChannelLayoutAuto,
        Left,
        Right,
        Mono,
        Stereo
    };

    enum BufferMode{
        Time = AVCodec2::BufferMode::Time,//ms
        Packet = AVCodec2::BufferMode::Packet
    };

    enum SynchMode{
        AUDIO, //以音频时间为基准，默认
        TIMER //以系统时间为基准
    };

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
    Status status() const;

    bool renderFirstFrame() const;
    void setRenderFirstFrame(bool);

    int bufferSize() const;
    void setBufferSize(int size);

    int volume() const;
    void setVolume(int vol);

    float playRate() const;
    void setPlayRate(float);

    int pos() const;
    int duration() const;

    BufferMode bufferMode() const;
    void setBufferMode(BufferMode mode);

    //QQmlParserStatus
    virtual void classBegin();
    virtual void componentComplete();

    /////

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


    void updateVideoFrame(const char*,VideoFormat*);
public slots:
    void play();
    void pause();
    void stop();
    void restart();
    void seek(int time);
private slots :
    void updateAudioFormat(QAudioFormat);
    void updateAudioFrame(const QByteArray &buffer);
    void exec();
    void canRenderFirstFrame();
    void requestAudioData();
    void statusChanged(AVCodec2::Status);
    void slotDurationChanged(int);
    /** 更新下载速率bytes len */
    void updateInternetSpeed(int speed);
private :
    void wakeupPlayer();
private:
    AVCodec2 *mCodec;
    Status mStatus;
    SynchMode mSynchMode;

    bool mAutoLoad;
    bool mAutoPlay;
    bool mRrnderFirstFrame;
    BufferMode mBufferMode;
    int mBufferSize;
    float mVolume;
    int mDuration;
    int mPos;
    int mSeekTime;

    QString mSource;

    /** 组件是否加载完成 */
    bool mComplete;
    /** 视频播放控制线程 */
    AVPlayerThread mThread;


    QMutex mAudioMutex;
    QAudioOutput* mAudio;
    QBuffer mBuffer;
    QIODevice *mAudioBuffer;
    QTimer m_audioPushTimer;
    int mLastTime;
    bool m_isDestroy;
    bool mIsPaused;
    bool mIsPlaying;

    QWaitCondition mCondition;
    QMutex mMutex;
    bool mIsAudioWaiting;
};

#endif // AVPLAYER_H
