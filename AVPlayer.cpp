#include "AVPlayer.h"

AVPlayer::AVPlayer()
    : mDecoder(new AVDecoder)
    , mComplete(false)
    , mAutoLoad(true)
    , mAutoPlay(false)
    , mAudio(NULL)
    , mRrnderFirstFrame(true)
    , mLastTime(0)
    , mIsDestroy(false)
    , mStatus(AVDefine::AVMediaStatus_UnknownStatus)
    , mSynchMode(AVDefine::AVSynchMode_Audio_Video)
    , mVolume(1.0)
    , mDuration(0)
    , mPos(0)
    , mIsPaused (true)
    , mIsPlaying (false)
    , mSeekTime(0)
    , mPlaybackState(AVDefine::AVPlayState_Stopped)
    , mAudioTimer(new AVTimer(this))
    , mAudioBuffer(NULL)
    , mIsSeeked(true)
    , mIsSetPlayRate(false)
    , mIsSetPlayRateBeforeIsPaused(false)
    , mIsAudioWaiting(NULL)
    , mRenderData(NULL)
    , mIsClickedPlay(false)
    , mpreview(false)
    , msourceWidth(-1)
    , msourceHeight(-1)
    , mkdMode(AVDefine::AVKDMode_Default)
    , mdecodecMode(AVDefine::AVDecodeMode_Default)
{
    mDecoder->setMediaCallback(this);
}

AVPlayer::~AVPlayer(){
    mAudioTimer->stop();
    delete mAudioTimer;
    mIsDestroy = true;

    mCondition.wakeAll();

    mThread.clearAllTask();
    mThread.stop();
    int i = 0;
    while(!mThread.isRunning() && i++ < 200){
        QThread::msleep(1);
    }

    mThread2.clearAllTask();
    mThread2.stop();
    i = 0;
    while(!mThread2.isRunning() && i++ < 200){
        QThread::msleep(1);
    }

    if(mDecoder != NULL){
//        delete mDecoder;
        mDecoder->deleteLater();
        mDecoder = NULL;
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
    if(mDecoder){
        mDecoder->setpreview(this->getpreview());
        mDecoder->setdecodecMode(mdecodecMode);
        if(mDecoder->getPlayPath() != mSource)
            mDecoder->setFilename(mSource);
        else{
            if (!mSource.isEmpty() && (mAutoLoad || mAutoPlay)) {
                if (mAutoLoad && mDecoder)
                    mDecoder->load();
            }
        }
    }
    mComplete = true;
}

void AVPlayer::play(){
    if(getIsPlaying()){
        if(getIsPaused())
            restart();
        return;
    }

    if(mDecoder)
        mDecoder->setpreview(this->getpreview());

    mIsClickedPlay = true;
    mAudioMutex.lock();
    if(mAudio == NULL){
        mAudioMutex.unlock();
        if(mDecoder)
            mDecoder->load();
        return;
    }

    mAudio->setVolume(mVolume);
    mAudioBufferMutex.lock();
    mAudioBuffer = mAudio->start();
    if(mAudio->error() != QAudio::NoError){
//        mAudioBufferMutex.unlock();
//        mAudioMutex.unlock();
//        return;
    }
//    qDebug() << "------------------ : " << mAudio->error() << mAudioBuffer << mAudio->format();
    mAudioBufferMutex.unlock();

    if(!getpreview()){ //预览时，不播放声音数据
        if(mAudio->error() == QAudio::NoError){
            mAudioTimer->begin();
        }
    }
    mAudioMutex.unlock();

    setIsPlaying(true);
    setIsPaused(false);
    mCondition.wakeAll();
    wakeupPlayer();
    seek(0);

    mPlaybackState = AVDefine::AVPlayState_Playing;
    emit playStatusChanged();
}

void AVPlayer::pause(){
    if(getIsPaused() || !getIsPlaying())
        return;
    setIsPaused(true);

    mThread.clearAllTask(AVPlayerTask::AVPlayerTaskCommand_Render); //清除渲染任务

    mAudioTimer->stop();
    mAudioMutex.lock();
    if(mAudio && mAudio->state() == QAudio::ActiveState){
        mAudio->suspend();
    }
    mAudioMutex.unlock();

    mPlaybackState = AVDefine::AVPlayState_Paused;
    emit playStatusChanged();
}

void AVPlayer::stop(){
    if(!getIsPlaying())
        return;
    mSeekTime = 0;
    setIsPaused(true);
    setIsPlaying(false);
    mThread.clearAllTask();
    mAudioTimer->stop();

    mAudioBufferMutex.lock();
    mAudioBuffer = NULL;
    mAudioBufferMutex.unlock();

    mAudioMutex.lock();
    if(mAudio && mAudio->state() != QAudio::StoppedState)
        mAudio->stop();
    mAudioMutex.unlock();

    if(mDecoder)
        mDecoder->stop();
    mCondition.wakeAll();

    mPlaybackState = AVDefine::AVPlayState_Stopped;
    emit playStatusChanged();
    mPos = mDuration;
}

void AVPlayer::restart(){
    if(!getIsPlaying())
        return;

    if(getIsPlaying() && !getIsPaused())
        return;

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

    if(!getpreview()){ //预览时，不播放声音数据
        if(mAudio->error() == QAudio::NoError){
            mAudioTimer->begin();
        }
    }

    mAudioMutex.unlock();
    wakeupPlayer();
    mCondition.wakeAll();
    mPlaybackState = AVDefine::AVPlayState_Playing;

    emit playStatusChanged();
}

void AVPlayer::seek(int time){
    if(mDecoder && !mDecoder->hasAudio() && !mDecoder->hasVideo())
        return;
    if(!getIsPlaying()){
        play();
    }
    mThread2.clearAllTask();//拖动前，删除待处理的任务
    mThread2.addTask(new AVPlayerTask(this,AVPlayerTask::AVPlayerTaskCommand_Seek,time));
}

void AVPlayer::showFrameByPosition(int time){

}

void AVPlayer::seekImpl(int time){
    if(mDecoder && !mDecoder->hasAudio() && !mDecoder->hasVideo())
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
    if(mDecoder)
        mDecoder->seek(time);
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

    stop();
    mAudioMutex.lock();
    if(mAudio != NULL){
        delete mAudio;
        mAudio = NULL;
    }
    mAudioMutex.unlock();

    if(mDecoder){
        mDecoder->setpreview(this->getpreview());
        mDecoder->setdecodecMode(mdecodecMode);
    }

    if(mComplete){
        if(mDecoder)
            mDecoder->setFilename(mSource);
    }
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
    if(mDecoder)
        return mComplete ? mDecoder->hasAudio() : mComplete;
    return false;
}
bool AVPlayer::hasVideo() const{
    if(mDecoder)
        return mComplete ? mDecoder->hasVideo() : mComplete;
    return false;
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

int AVPlayer::getplaySpeedRate() const{
    if(mDecoder)
        return mDecoder->getPlayRate();
    return (int)AVDefine::AVPlaySpeedRate_Normal;
}
void AVPlayer::setplaySpeedRate(int playRate){
    mThread2.clearAllTask((int)AVPlayerTask::AVPlayerTaskCommand_SetPlayRate);
    mThread2.addTask(new AVPlayerTask(this,AVPlayerTask::AVPlayerTaskCommand_SetPlayRate,playRate));
}

void AVPlayer::slotSetPlayRate(int playRate){
    if(mIsSetPlayRate){
        if(mThread2.size((int)AVPlayerTask::AVPlayerTaskCommand_SetPlayRate) == 0){
            mThread2.addTask(new AVPlayerTask(this,AVPlayerTask::AVPlayerTaskCommand_SetPlayRate,playRate));
        }
        return;
    }

    if(mDecoder && mDecoder->getPlayRate() == playRate){
        return;
    }

    mIsSetPlayRate = true;
    mIsSetPlayRateBeforeIsPaused = getIsPaused();
    if(!getIsPaused())
        pause();
    if(mDecoder)
        mDecoder->setPlayRate(playRate);

    emit playSpeedRateChanged();
}

bool AVPlayer::getaccompany()const{
    if(mDecoder)
        return mDecoder->getAccompany();
    return false;
}
void AVPlayer::setaccompany(bool flag){
    if(mDecoder){
        mDecoder->setAccompany(flag);
        emit accompanyChanged();
    }
}

int AVPlayer::getchannelLayout()const{
    if(mDecoder)
        return mDecoder->getAudioChannel();
    return (int)AVDefine::AVChannelLayout_Default;
}

void AVPlayer::setchannelLayout(int value){
    if(mDecoder){
        mDecoder->setAudioChannel((AVDefine::AVChannelLayout)value);
        emit channelLayoutChanged();
    }
}

void AVPlayer::setkdMode(int value){
    mkdMode = value;
    emit kdModeChanged();
}

void AVPlayer::setminimumBufferSize(int value){
    if(value < 3000)
        value = 3000;
    if(mDecoder)mDecoder->setminimumBufferSize(value);
}

void AVPlayer::setmaximumBufferSize(int value){
    if(mDecoder)mDecoder->setmaximumBufferSize(value);
}

void AVPlayer::setdecodecMode(int value){
    mdecodecMode = value;
    if(mDecoder)
        mDecoder->setdecodecMode(value);
}

int AVPlayer::getdecodecMode() const{
    if(mDecoder)
        return mDecoder->getdecodecMode();
    return (int)AVDefine::AVDecodeMode_Soft;
}

QVector<int> AVPlayer::getsupportDecodecModeList() const{
    if(mDecoder)mDecoder->getsupportDecodecModeList();
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
    if(!getpreview()){ //如果是预览，则不创建
        mAudio = new QAudioOutput(format);
    }
    mAudioMutex.unlock();

    if(isRestart || (mIsSetPlayRate && !mIsSetPlayRateBeforeIsPaused))
        restart();

    mIsSetPlayRate = false;
}

void AVPlayer::mediaUpdateAudioFrame(const QByteArray &buffer){
    if(!getIsPlaying())
        return;
    mAudioBufferMutex.lock();
    if(mAudioBuffer != NULL){
        mAudioBuffer->write(buffer);
        mPos = mAudio->processedUSecs() / 1000;
        if(mDecoder)
            mPos *= mDecoder->getRealPlayRate();
        mPos += mSeekTime;
    }

    mAudioBufferMutex.unlock();

    emit positionChanged();
}

void AVPlayer::requestAudioData(){
    if(getIsPaused())
        return;

    bool locked = mAudioMutex.tryLock();
    int chunks = 0;
    int periodSize = 0;
    if (mAudio && mAudio->state() != QAudio::StoppedState) {
        periodSize = mAudio->periodSize();
        chunks = mAudio->bytesFree()/periodSize;
    }

    if(locked){
        mAudioMutex.unlock();
    }

    if(chunks){
        if(mDecoder)
            mDecoder->requestAudioNextFrame(periodSize);
    }
}

void AVPlayer::wakeupPlayer(){
    if(hasVideo()){
        mThread.addTask(new AVPlayerTask(this,AVPlayerTask::AVPlayerTaskCommand_Render));
    }
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
    if(mIsDestroy || getIsPaused() || !mAudio || mDecoder == NULL){
        return;
    }

    int currentVideoTime = mDecoder->requestRenderNextFrame();

    if(mDecoder == NULL)return;
    if(mDecoder->isVideoPlayed())
    {
        return;
    }
    if(mDecoder == NULL)return;
    int nextTime = mDecoder->nextTime();
    mAudioMutex.lock();
    int currentTime = mAudio->processedUSecs() / 1000;
//    qDebug() << "------------- current time : " << currentTime << ":" << nextTime;
    if(!hasAudio()){ //如果没有音频，则不使用音频的时间挫
        currentTime = currentVideoTime;
        mPos = currentTime;
        emit positionChanged();
    }

    mAudioMutex.unlock();//----------- 5
    if(mDecoder == NULL)return;
    currentTime *= mDecoder->getRealPlayRate();
    if(hasAudio())
        currentTime += mSeekTime;
    int sleepTime = nextTime - currentTime < 0 ? 0 : nextTime - currentTime;
//    qDebug() << "------------------ sleepTime : " << sleepTime << ":" << mAudio->processedUSecs() / 1000 << ":" << nextTime << ":" << currentTime;

    if(hasAudio() && mkdMode == AVDefine::AVKDMode_Audio_Wait && mDecoder->getPlayRate() == AVDefine::AVPlaySpeedRate_Normal){
        if(currentTime - nextTime >= 1000 && !mIsAudioWaiting){ //某些视频解码太耗时，误差太大的话，先将音频暂停，再继续播放
            mAudioMutex.lock();
            if(mAudio)
                mAudio->suspend();
            mAudioMutex.unlock();
            mIsAudioWaiting = true;
            mAudioTimer->stop();
        }else if(mIsAudioWaiting && currentTime - nextTime < 1000){
            mAudioMutex.lock();
            if(mAudio)
                mAudio->resume();
            mIsAudioWaiting = false;
            if(!getpreview()){ //预览时，不播放声音数据
                if(mAudio && mAudio->error() == QAudio::NoError)
                    mAudioTimer->begin();
            }
            mAudioMutex.unlock();
        }
    }else if(mkdMode == AVDefine::AVKDMode_ThrowAway_Video || mDecoder->getPlayRate() != AVDefine::AVPlaySpeedRate_Normal){
        if(currentTime - nextTime >= 1000 && !mIsAudioWaiting){
            if(mDecoder != NULL)
                mDecoder->throwAwaysFrameToTime(currentTime);
        }
    }

    if(sleepTime > 0){
        mMutex.lock();
        mCondition.wait(&mMutex,sleepTime);
        mMutex.unlock();
    }
    mLastTime = nextTime;

    if(mIsDestroy || getIsPaused() || !mAudio){
        return;
    }
    if(mDecoder == NULL)return;
    if(mDecoder->isVideoPlayed())
    {
        return;
    }

    wakeupPlayer();
}

void AVPlayer::mediaCanRenderFirstFrame(){
    if(mRrnderFirstFrame && hasVideo()){
        if(mDecoder)
            mDecoder->renderFirstFrame(); //获取第一帧，并渲染
    }
}

void AVPlayer::mediaStatusChanged(AVDefine::AVMediaStatus status){
    emit statusChanged();
    switch(status){
        case AVDefine::AVMediaStatus_Seeking :
        case AVDefine::AVMediaStatus_Buffering :{
//            qDebug() <<"buffering paused";
            pause();
            break;
        }
        case AVDefine::AVMediaStatus_Buffered :{
//            qDebug() <<"buffered played:";
            if(mStatus != AVDefine::AVMediaStatus_Seeking && (this->autoPlay() || mIsClickedPlay))
                play();
            mIsClickedPlay = false;
            break;
        }
        case AVDefine::AVMediaStatus_Played :{
//            qDebug() <<"播放完成";
            stop();
            emit positionChanged();
            emit playCompleted();
            break;
        }

        case AVDefine::AVMediaStatus_Seeked :{
//            qDebug() <<"拖动完成" << mThread2.size();
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
    mRenderData = (VideoFormat *)f;
    if(mRenderData->width != msourceWidth){
        msourceWidth = mRenderData->width;
        emit sourceWidthChanged();
    }

    if(mRenderData->height != msourceHeight){
        msourceHeight = mRenderData->height;
        emit sourceHeightChanged();
    }
    emit updateVideoFrame(mRenderData);
}

void AVPlayer::mediaHasAudioChanged(){
    emit hasAudioChanged();
}

void AVPlayer::mediaHasVideoChanged(){
    emit hasVideoChanged();
}

AVDefine::AVPlayState AVPlayer::getPlaybackState(){
    return mPlaybackState;
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
