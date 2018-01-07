#include "avplayer.h"
#include <QTimer>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>

AVPlayer::AVPlayer()
    : mCodec(NULL)
    , mComplete(false)
    , mAutoLoad(true)
    , mAutoPlay(false)
    , mAudio(NULL)
    , mBufferMode(BufferMode::Time)
    , mBufferSize(5000)
    , mRrnderFirstFrame(true)
    , mLastTime(0)
    , m_isDestroy(false)
    , mStatus(AVPlayer::UnknownStatus)
    , mSynchMode(AUDIO)
    , mVolume(1.0)
    , mDuration(0)
    , mPos(0)
    , mIsPaused (true)
    , mIsPlaying (false)
    , mSeekTime(0)
    , mIsAudioWaiting(false)
{
    mVolume = 0;
    mCodec = new AVCodec2;

    mCodec->setBufferMode((AVCodec2::BufferMode)mBufferMode);
    mCodec->setBufferSize(mBufferSize);

    connect(mCodec,SIGNAL(updateVideoFrame(const char*,VideoFormat*)),this,SIGNAL(updateVideoFrame(const char*,VideoFormat*)),Qt::DirectConnection);


    connect(mCodec,SIGNAL(hasAudioChanged()),this,SIGNAL(hasAudioChanged()));
    connect(mCodec,SIGNAL(hasVideoChanged()),this,SIGNAL(hasVideoChanged()));

    connect(mCodec,SIGNAL(canRenderFirstFrame()),this,SLOT(canRenderFirstFrame()));
    connect(mCodec,SIGNAL(statusChanged(AVCodec2::Status)),this,SLOT(statusChanged(AVCodec2::Status)));
    connect(mCodec,SIGNAL(updateAudioFormat(QAudioFormat)),this,SLOT(updateAudioFormat(QAudioFormat)));
    connect(mCodec,SIGNAL(updateAudioFrame(QByteArray)),this,SLOT(updateAudioFrame(QByteArray)));
    connect(mCodec,SIGNAL(durationChanged(int)),this,SLOT(slotDurationChanged(int)));
    connect(mCodec,SIGNAL(updateInternetSpeed(int)),this,SLOT(updateInternetSpeed(int)));


    mThread.moveToThread(&mThread);
    connect(&mThread,SIGNAL(onPlayerEvent()),this,SLOT(exec()),Qt::DirectConnection);
    mThread.start();
    QTimer *m_timer = new QTimer();
    connect(m_timer,SIGNAL(timeout()),mCodec,SLOT(print()));
    m_timer->start(50);
}

AVPlayer::~AVPlayer(){
    m_isDestroy = true;
    mThread.quit();
    mThread.wait();
}

void AVPlayer::play(){
    if(mIsPlaying)
        return;
    mAudioMutex.lock();
    static bool isLoaded = false;
    if(!isLoaded){
        mAudioMutex.unlock();
        mCodec->load();
        return;
    }
    /*
    if(mAudio == NULL){
        mAudioMutex.unlock();
        mCodec->load();
        return;
    }
    */
    if(mAudio){
        mAudio->setVolume(mVolume);
        mAudioBuffer = mAudio->start();
        requestAudioData();
        m_audioPushTimer.start(20);
    }
    mAudioMutex.unlock();

    mIsPlaying = true;
    mIsPaused = false;
    mCondition.wakeAll();
    wakeupPlayer();
    seek(0);
}

void AVPlayer::pause(){
    if(mIsPaused)
        return;
    mIsPaused = true;
    m_audioPushTimer.stop();
    mAudioMutex.lock();
    if(mAudio)
        mAudio->suspend();
    mAudioMutex.unlock();
}

void AVPlayer::stop(){
    if(!mIsPlaying)
        return;
    mSeekTime = 0;
    mIsPaused = true;
    mIsPlaying = false;
    m_audioPushTimer.stop();
    mAudioMutex.lock();
    if(mAudio)
        mAudio->stop();
    mAudioMutex.unlock();
    mCondition.wakeAll();
}

void AVPlayer::restart(){
    if(mIsPlaying && !mIsPaused)
        return;
    mIsPaused = false;
    mAudioMutex.lock();
    if(mAudio){
        if(mAudio->state() == QAudio::StoppedState){
            mAudio->setVolume(mVolume);
            mAudioBuffer = mAudio->start();
            requestAudioData();
        }else{
            mAudio->resume();
        }
    }
    mAudioMutex.unlock();
    m_audioPushTimer.start(20);
    wakeupPlayer();
    mCondition.wakeAll();
}

void AVPlayer::seek(int time){
    if(!mCodec->hasAudio() && !mCodec->hasVideo())
        return;

    int dur = duration();
    if((time + 1000) >= dur){
        time = dur - 1000;
    }
    mSeekTime = time;
    pause();
    mAudioMutex.lock();
    if(mAudio)
        mAudio->stop();
    mAudioMutex.unlock();
    mCodec->seek(time);
}

QString AVPlayer::source() const{
    return mSource;
}
void AVPlayer::setSource(const QString &source){
    if(mSource == source)
        return;
    if(source.mid(0,8) == "file:///"){
        mSource = source.mid(8,source.size() - 8);
    }else{
         mSource = source;
    }

    mAudioMutex.lock();
    if(mAudio != NULL){
        mAudio->deleteLater();
        mAudio = NULL;
    }
    mAudioMutex.unlock();
    mCodec->setFilename(mSource);
    emit sourceChanged();
}

bool AVPlayer::autoLoad() const{
    return mAutoLoad;
}
void AVPlayer::setAutoLoad(bool flag){
    mAutoLoad = flag;
    emit autoLoadChanged();
}

bool AVPlayer::autoPlay() const{
    return mAutoPlay;
}
void AVPlayer::setAutoPlay(bool flag){
    mAutoPlay = flag;
    emit autoPlayChanged();
}

bool AVPlayer::hasAudio() const{
    return mComplete ? mCodec->hasAudio() : mComplete;
}
bool AVPlayer::hasVideo() const{
    return mComplete ? mCodec->hasVideo() : mComplete;
}

AVPlayer::Status AVPlayer::status() const{
    return mStatus;
}

bool AVPlayer::renderFirstFrame() const{
    return mRrnderFirstFrame;
}
void AVPlayer::setRenderFirstFrame(bool flag){
    mRrnderFirstFrame = flag;
}

int AVPlayer::bufferSize() const{
    return mBufferSize;
}
void AVPlayer::setBufferSize(int size){
    mBufferSize = size;
    mCodec->setBufferSize(mBufferSize);
}

int AVPlayer::volume() const{
    return mVolume * 100;
}
void AVPlayer::setVolume(int vol){
    mAudioMutex.lock();
    if(mAudio)
        mAudio->setVolume(vol * 1.0 / 100);
    mAudioMutex.unlock();
    mVolume = vol * 1.0 / 100;
}

float AVPlayer::playRate() const{
    return mCodec->getPlayRate();
}
void AVPlayer::setPlayRate(float playRate){
    mCodec->setPlayRate(playRate);
}

AVPlayer::BufferMode AVPlayer::bufferMode() const{
    return mBufferMode;
}
void AVPlayer::setBufferMode(BufferMode mode){
    mBufferMode = mode;
    mCodec->setBufferMode((AVCodec2::BufferMode)mBufferMode);
}

void AVPlayer::classBegin(){

}

void AVPlayer::componentComplete(){
    if (!mSource.isEmpty() && (mAutoLoad || mAutoPlay)) {
        if (mAutoLoad)
            mCodec->load();
        if (mAutoPlay)
            mCodec->play();
    }
    mComplete = true;
}

void AVPlayer::updateAudioFormat(QAudioFormat format){
    return;
    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported(format)) {
        qWarning() << "Raw audio format not supported by backend, cannot play audio.";
        return;
    }
    pause();

    mAudioMutex.lock();
    if(mAudio != NULL){
        disconnect(&m_audioPushTimer, SIGNAL(timeout()),this, SLOT(requestAudioData()));
        mSeekTime = mPos;
        mAudio->deleteLater();
    }

    mBuffer.close();
    mBuffer.open(QBuffer::ReadWrite);

    if(false){
        mAudio = new QAudioOutput(format);
        mAudioMutex.unlock();
        connect(&m_audioPushTimer, SIGNAL(timeout()),this, SLOT(requestAudioData()));
    }

    restart();
}

void AVPlayer::updateAudioFrame(const QByteArray &buffer){
    if(mAudioBuffer && mAudio){
        mAudioBuffer->write(buffer);
        mPos = mAudio->processedUSecs() / 1000;
        mPos *= mCodec->getPlayRate();
        mPos += mSeekTime;
        emit positionChanged();
    }
}

void AVPlayer::requestAudioData(){
    if (mAudio && mAudio->state() != QAudio::StoppedState) {
        int chunks = mAudio->bytesFree()/mAudio->periodSize();
        if(chunks){
            mCodec->requestAudioNextFrame(mAudio->periodSize());
        }
    }
}

void AVPlayer::wakeupPlayer(){
    QCoreApplication::postEvent(&mThread,new QEvent(QEvent::Type(EVENT_PLAYER_DRAW)),Qt::HighEventPriority);
}


void AVPlayer::exec(){
//    if(m_isDestroy || mIsPaused || !mAudio)
//        return;
//    mCodec->renderNextFrame();
//    int nextTime = mCodec->getCurrentVideoTime();
//    mAudioMutex.lock();
//    int currentTime = mAudio->processedUSecs() / 1000;
//    mAudioMutex.unlock();
//    currentTime *= mCodec->getPlayRate();
//    currentTime += mSeekTime;

//    if(currentTime - nextTime >= 1000 && !mIsAudioWaiting){ //某些视频解码太耗时，误差太大的话，先将音频暂停，在继续播放
//        mAudio->suspend();
//        mIsAudioWaiting = true;
//    }else if(mIsAudioWaiting && currentTime - nextTime < 1000){
//        mAudio->resume();
//        mIsAudioWaiting = false;
//    }
//    int sleepTime = nextTime - currentTime < 1 ? 1 : nextTime - currentTime;

//    if(sleepTime > 1){
//        mMutex.lock();
//        mCondition.wait(&mMutex,sleepTime);
//        mMutex.unlock();
//    }
//    mLastTime = nextTime;
//    wakeupPlayer();

    if(m_isDestroy || mIsPaused)
        return;
    mCodec->renderNextFrame();
    int sleepTime = 35;

    if(sleepTime > 1){
        mMutex.lock();
        mCondition.wait(&mMutex,sleepTime);
        mMutex.unlock();
    }
    wakeupPlayer();
}

void AVPlayer::canRenderFirstFrame(){
    if(mRrnderFirstFrame)
        mCodec->renderFirstFrame(); //获取第一帧，并渲染
}

void AVPlayer::statusChanged(AVCodec2::Status status){
    switch(status){
        case AVCodec2::Buffering :{
            qDebug() <<"buffering paused";
            pause();
            break;
        }
        case AVCodec2::Buffered :{
            qDebug() <<"buffered played";
            play();
            break;
        }
    case AVCodec2::Played :{
            qDebug() <<"播放完成";
            stop();
            break;
        }

    case AVCodec2::Seeked :{
            qDebug() <<"拖动完成";
            restart();
            break;
        }
    }
    mStatus = (AVPlayer::Status)status;
    emit statusChanged();
}

void AVPlayer::slotDurationChanged(int duration){
    mDuration = duration;
    emit durationChanged();
}

int AVPlayer::pos() const{
    return mPos;
}
int AVPlayer::duration() const{
    return mDuration;
}

void AVPlayer::updateInternetSpeed(int speed){
    //qDebug() << " download speed : " << speed * 1.0 / 1024 << "kb/s";
}
