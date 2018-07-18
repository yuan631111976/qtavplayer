#include "AVPlayer.h"

AVPlayer::AVPlayer()
    : AVMediaPlayer(0)
    , mCodec(NULL)
    , mComplete(false)
    , mAutoLoad(true)
    , mAutoPlay(false)
    , mAudio(NULL)
    , mMediaBufferMode(AVDefine::MediaBufferMode_Time)
    , mBufferSize(5000)
    , mRrnderFirstFrame(true)
    , mLastTime(0)
    , mIsDestroy(false)
    , mStatus(AVDefine::MediaStatus_UnknownStatus)
    , mSynchMode(AVDefine::AUDIO)
    , mVolume(1.0)
    , mDuration(0)
    , mPos(0)
    , mIsPaused (true)
    , mIsPlaying (false)
    , mSeekTime(0)
    , mPlayerCallback(NULL)
    , mPlaybackState(AVDefine::StoppedState)
    , mAudioTimer(new AVTimer(this))
    , mAudioBuffer(NULL)
    , mIsSeeked(true)
    , mIsSetPlayRate(false)
    , mIsSetPlayRateBeforeIsPaused(false)
    , mIsAudioWaiting(NULL)
    , mIsDecoded(false)
    , mRenderData(NULL)
    , mIsClickedPlay(true)
{
    mCodec = new AVCodec2;
    mCodec->setMediaCallback(this);
    mCodec->setMediaBufferMode(mMediaBufferMode);
    mCodec->setBufferSize(mBufferSize);
    mComplete = true;
}

AVPlayer::~AVPlayer(){
    mAudioTimer->stop();
    delete mAudioTimer;

    mIsDestroy = true;
    mCondition.wakeAll();

    mThread.stop();
    mThread2.stop();

    if(mCodec != NULL){
        delete mCodec;
    }

    delete mAudio;
    mAudio = NULL;

    mAudioBufferMutex.lock();
    if(mAudioBuffer != NULL){
        mAudioBuffer->close();
        delete mAudioBuffer;
        mAudioBuffer = NULL;
    }
    mAudioBufferMutex.unlock();
}

void AVPlayer::classBegin(){

}

void AVPlayer::componentComplete(){
    if (!mSource.isEmpty() && (mAutoLoad || mAutoPlay)) {
        if (mAutoLoad)
            mCodec->load();
//        if (mAutoPlay)
//            mCodec->play();
    }
    mComplete = true;
}

void AVPlayer::play(){
    if(getIsPlaying()){
        if(getIsPaused())
            restart();
        return;
    }

    mIsClickedPlay = true;
    mAudioMutex.lock();
    if(mAudio == NULL){
        mAudioMutex.unlock();
        mCodec->load();
        return;
    }

    mAudio->setVolume(mVolume);
    mAudioBufferMutex.lock();
    mAudioBuffer = mAudio->start();
    mAudioBufferMutex.unlock();
    mAudioMutex.unlock();

    mAudioTimer->begin();
//    mIsPlaying = true;
//    mIsPaused = false;
    setIsPlaying(true);
    setIsPaused(false);
    mCondition.wakeAll();
    wakeupPlayer();
    seek(0);

    mPlaybackState = AVDefine::PlayingState;
    if(mPlayerCallback != NULL)
        mPlayerCallback->playStatusChanged(AVDefine::PlayingState);
    emit playStatusChanged();
}

void AVPlayer::pause(){
    if(getIsPaused() || !getIsPlaying())
        return;
    setIsPaused(true);
//    mIsPaused = true;
    mAudioTimer->stop();
    mAudioMutex.lock();

    if(mAudio && mAudio->state() == QAudio::ActiveState){
//        mAudioBufferMutex.lock();
//        mAudioBuffer = NULL;
//        mAudioBufferMutex.unlock();
        mAudio->suspend();
    }
    mAudioMutex.unlock();

    mPlaybackState = AVDefine::PausedState;
    if(mPlayerCallback != NULL)
        mPlayerCallback->playStatusChanged(AVDefine::PausedState);
    emit playStatusChanged();
}

void AVPlayer::stop(){
    if(!getIsPlaying())
        return;
    mSeekTime = 0;
//    mIsPaused = true;
//    mIsPlaying = false;
    setIsPaused(true);
    setIsPlaying(false);
    mAudioTimer->stop();

    mAudioBufferMutex.lock();
    mAudioBuffer = NULL;
    mAudioBufferMutex.unlock();

    mAudioMutex.lock();
    if(mAudio && mAudio->state() != QAudio::StoppedState)
        mAudio->stop();
    mAudioMutex.unlock();
    mCondition.wakeAll();

    mPlaybackState = AVDefine::StoppedState;
    if(mPlayerCallback != NULL)
        mPlayerCallback->playStatusChanged(AVDefine::StoppedState);
    emit playStatusChanged();
}

void AVPlayer::restart(){
    if(!getIsPlaying())
        return;
    if(getIsPlaying() && !getIsPaused())
        return;

//    mIsPaused = false;
    setIsPaused(false);
    mAudioMutex.lock();
    if(mAudio){
        if(mAudio->state() == QAudio::StoppedState){
            mAudio->setVolume(mVolume);
            mAudioBufferMutex.lock();
            mAudioBuffer = mAudio->start();
            mAudioBufferMutex.unlock();
            requestAudioData();
        }else{
            if(mAudio->state() == QAudio::SuspendedState){
                mAudio->resume();
            }
        }
    }
    mAudioMutex.unlock();
    mAudioTimer->begin();
    wakeupPlayer();
    mCondition.wakeAll();
    mPlaybackState = AVDefine::PlayingState;
    if(mPlayerCallback != NULL)
        mPlayerCallback->playStatusChanged(AVDefine::PlayingState);
    emit playStatusChanged();
}

void AVPlayer::seek(int time){
    if(!mCodec->hasAudio() && !mCodec->hasVideo())
        return;
    if(!getIsPlaying()){
        play();
    }
    mThread2.clearAllTask();//拖动前，删除待处理的任务
    mThread2.addTask(new AVPlayerTask(this,AVPlayerTask::AVPlayerTaskCommand_Seek,time));

}

void AVPlayer::seekImpl(int time){
    if(!mCodec->hasAudio() && !mCodec->hasVideo())
        return;

    mIsSeekedMutex.lock();
    if(!mIsSeeked){
        if(mThread2.size((int)AVPlayerTask::AVPlayerTaskCommand_Seek) == 0){
            mThread2.addTask(new AVPlayerTask(this,AVPlayerTask::AVPlayerTaskCommand_Seek,time));
            mIsSeekedMutex.unlock();
            return;
        }
    }
    mIsSeeked = false;
    mIsSeekedMutex.unlock();
    pause();

    mAudioBufferMutex.lock();
    mAudioBuffer = NULL;
    mAudioBufferMutex.unlock();

    mAudioMutex.lock();
    if(mAudio){
        mAudio->stop();
    }
    mAudioMutex.unlock();

    int dur = duration();
    if((time + 1000) >= dur){
        time = dur - 1000;
    }
    mSeekTime = time;
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
        delete mAudio;
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

int AVPlayer::status() const{
    return (int)mStatus;
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
    emit volumeChanged();
}

float AVPlayer::playRate() const{
    return mCodec->getPlayRate();
}
void AVPlayer::setPlayRate(float playRate){
    if(playRate == 0)
        return;
    mThread2.clearAllTask((int)AVPlayerTask::AVPlayerTaskCommand_SetPlayRate);
    mThread2.addTask(new AVPlayerTask(this,AVPlayerTask::AVPlayerTaskCommand_SetPlayRate,playRate));
    emit playRateChanged();
}

void AVPlayer::slotSetPlayRate(float playRate){
    if(mIsSetPlayRate){
        if(mThread2.size((int)AVPlayerTask::AVPlayerTaskCommand_SetPlayRate) == 0){
            mThread2.addTask(new AVPlayerTask(this,AVPlayerTask::AVPlayerTaskCommand_SetPlayRate,playRate));
        }
        return;
    }

    if(mCodec->getPlayRate() == playRate){
        return;
    }

    mIsSetPlayRate = true;
    mIsSetPlayRateBeforeIsPaused = getIsPaused();
    if(!getIsPaused())
        pause();
    mCodec->setPlayRate(playRate);
}

int AVPlayer::getMediaBufferMode() const{
    return (int)mMediaBufferMode;
}
void AVPlayer::setMediaBufferMode(int mode){
    mMediaBufferMode = (AVDefine::MediaBufferMode)mode;
    mCodec->setMediaBufferMode(mMediaBufferMode);
}

void AVPlayer::mediaUpdateAudioFormat(const QAudioFormat &format){
    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported(format)) {
        return;
    }

    bool isRestart = !getIsPaused();
    if(!getIsPaused()){
        pause();
    }

    mAudioMutex.lock();
    if(mAudio != NULL){
        mSeekTime = mPos;
        delete mAudio;
    }
    mAudio = new QAudioOutput(format);
    mAudioMutex.unlock();

    if(isRestart || (mIsSetPlayRate && !mIsSetPlayRateBeforeIsPaused))
        restart();

    mIsSetPlayRate = false;
}

void AVPlayer::mediaUpdateAudioFrame(const QByteArray &buffer){
    mAudioBufferMutex.lock();
    if(mAudioBuffer != NULL){
        mAudioBuffer->write(buffer);
        mPos = mAudio->processedUSecs() / 1000;
        mPos *= mCodec->getPlayRate();
        mPos += mSeekTime;
    }
    mAudioBufferMutex.unlock();

    if(mPlayerCallback != NULL)
        mPlayerCallback->positionChanged(mPos);

    emit positionChanged();
}

void AVPlayer::requestAudioData(){
    if(getIsPaused())
        return;
//    mAudioMutex.lock();
    if (mAudio && mAudio->state() != QAudio::StoppedState) {
        int periodSize = mAudio->periodSize();
        int chunks = mAudio->bytesFree()/periodSize;
//        mAudioMutex.unlock();
        if(chunks){
            mCodec->requestAudioNextFrame(periodSize);
        }
    }else{
//        mAudioMutex.unlock();
    }
}

void AVPlayer::wakeupPlayer(){
    mThread.addTask(new AVPlayerTask(this,AVPlayerTask::AVPlayerTaskCommand_Render));
}

void AVPlayer::setIsPaused(bool paused){
    mStatusMutex.lock();
    mIsPaused = paused;
    mStatusMutex.unlock();
}

bool AVPlayer::getIsPaused(){
    mStatusMutex.lock();
    bool result = mIsPaused;
    mStatusMutex.unlock();
    return result;
}

void AVPlayer::setIsPlaying(bool playing){
    mStatusMutex.lock();
    mIsPlaying = playing;
    mStatusMutex.unlock();
}

bool AVPlayer::getIsPlaying(){
    mStatusMutex.lock();
    bool result = mIsPlaying;
    mStatusMutex.unlock();
    return result;
}

void AVPlayer::requestRender(){
    if(mIsDestroy || getIsPaused() || !mAudio){
        return;
    }
    mCodec->renderNextFrame();
    int nextTime = mCodec->getCurrentVideoTime();
    mAudioMutex.lock();
    int currentTime = mAudio->processedUSecs() / 1000;

    mAudioMutex.unlock();//----------- 5
    currentTime *= mCodec->getPlayRate();
    currentTime += mSeekTime;
    int sleepTime = nextTime - currentTime < 0 ? 0 : nextTime - currentTime;
//    qDebug() << "------------------ sleepTime : " << sleepTime;
    if(currentTime - nextTime >= 1000 && !mIsAudioWaiting){ //某些视频解码太耗时，误差太大的话，先将音频暂停，在继续播放
        mAudioBufferMutex.lock();
        mAudioBuffer = NULL;
        mAudioBufferMutex.unlock();
        mAudioMutex.lock();
        if(mAudio)
            mAudio->suspend();
        mAudioMutex.unlock();
        mIsAudioWaiting = true;
    }else if(mIsAudioWaiting && currentTime - nextTime < 1000){
        mAudioBufferMutex.lock();
        mAudioBuffer = NULL;
        mAudioBufferMutex.unlock();
        mAudioMutex.lock();
        if(mAudio)
            mAudio->resume();
        mAudioMutex.unlock();
        mIsAudioWaiting = false;
    }

    if(sleepTime > 0){
        mMutex.lock();
        mCondition.wait(&mMutex,sleepTime);
        mMutex.unlock();
    }
    mLastTime = nextTime;

    wakeupPlayer();
}

void AVPlayer::mediaCanRenderFirstFrame(){
    if(mRrnderFirstFrame){
        mIsDecoded = false;
        int i = 0;
        while(!mIsDecoded && i++ < 10){
            mCodec->renderFirstFrame(); //获取第一帧，并渲染
        }

    }
}

void AVPlayer::mediaStatusChanged(AVDefine::MediaStatus status){
    if(mPlayerCallback != NULL){
        mPlayerCallback->statusChanged(status);
    }
    emit statusChanged();
    switch(status){
        case AVDefine::MediaStatus_Seeking :
        case AVDefine::MediaStatus_Buffering :{
            //qDebug() <<"buffering paused";
            pause();
            break;
        }
        case AVDefine::MediaStatus_Buffered :{
            //qDebug() <<"buffered played";
            if(mStatus != AVDefine::MediaStatus_Seeking && (this->autoPlay() || mIsClickedPlay))
                play();
            mIsClickedPlay = false;
            break;
        }
        case AVDefine::MediaStatus_Played :{
            //qDebug() <<"播放完成";
            stop();
            if(mPlayerCallback != NULL){
                mPlayerCallback->positionChanged(duration());
            }
            emit positionChanged();
            emit playCompleted();
            break;
        }

        case AVDefine::MediaStatus_Seeked :{
            //qDebug() <<"拖动完成";
            mIsSeekedMutex.lock();
            mIsSeeked = true;
            mIsSeekedMutex.unlock();
            if(mThread2.size() == 0)
                restart();
            break;
        }
    }
    mStatus = status;
}

void AVPlayer::mediaDurationChanged(int duration){
    mDuration = duration;
    if(mPlayerCallback != NULL){
        mPlayerCallback->durationChanged(duration);
    }
    emit durationChanged();
}

int AVPlayer::pos() const{
    return mPos;
}
int AVPlayer::duration() const{
    return mDuration;
}

VideoFormat *AVPlayer::getRenderData(){
    return mRenderData;
}

void AVPlayer::mediaUpdateVideoFrame(void* f){
    if(mPlayerCallback != NULL){
        VideoFormat *format = (VideoFormat *)f;
        mPlayerCallback->updateVideoFrame(0,format->width,format->height,format->rotate,format->format);
    }
    mRenderData = (VideoFormat *)f;
    mIsDecoded = true;
    emit updateVideoFrame(mRenderData);
}

void AVPlayer::mediaHasAudioChanged(){
    emit hasAudioChanged();
}

void AVPlayer::mediaHasVideoChanged(){
    emit hasVideoChanged();
}


/** 设置文件路径 */
void AVPlayer::setFileName(const char *path){
    QString str(path);
    setSource(str);
}

/** 获取当前音量 */
int AVPlayer::getVolume(){
    return volume();
}

/** 设置播放速率(0.125 - 8)
*   0.125 以1/8的速度播放(慢速播放)
*   8 以8/1的速度播放(快速播放)
*   慢速播放公式(rate = 1.0 / 倍数(最大为8))
*   快速播放公式(rate = 1.0 * 倍数(最大为8))
*/
void AVPlayer::setPlaybackRate(float rate){
    setPlayRate(rate);
}

/** 获取当前播放速率 */
float AVPlayer::getPlaybackRate(){
    return mCodec->getPlayRate();
}

/** 获取当前视频的总时长 */
int AVPlayer::getDuration(){
    return duration();
}

/** 获取当前视频的播放位置 */
int AVPlayer::getPosition(){
    return pos();
}

/** 获取当前媒体状态 */
int AVPlayer::getCurrentMediaStatus(){
    return (int)mStatus;
}

/** 获取当前播放状态 */
int AVPlayer::getPlaybackState(){
    return (int)mPlaybackState;
}

/** 设置播放回调接口 */
void AVPlayer::setPlayerCallback(AVPlayerCallback *callback){
    this->mPlayerCallback = callback;
}


/** 现程实现 */
void AVPlayerTask::run(){
    switch(command){
    case AVPlayerTaskCommand_Render:
        mPlayer->requestRender();
        break;
    case AVPlayerTaskCommand_Seek:
        mPlayer->seekImpl(param);
        break;
    case AVPlayerTaskCommand_SetPlayRate:
        mPlayer->slotSetPlayRate(param);
        break;
    }
}
