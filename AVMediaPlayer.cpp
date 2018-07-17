#include "AVMediaPlayer.h"
#include "AVPlayer.h"
#include <QCoreApplication>


AVMediaPlayer::AVMediaPlayer()
    : mPlayer(new AVPlayer())
{
    QCoreApplication::addLibraryPath("./");
}

AVMediaPlayer::~AVMediaPlayer()
{

}

/** 删除当前对象 */
void AVMediaPlayer::del(){
    if(mPlayer != NULL){
        delete mPlayer;
        mPlayer = NULL;
    }
}

/** 设置播放回调接口 */
void AVMediaPlayer::setPlayerCallback(AVPlayerCallback *pCallback){
    if(mPlayer != NULL){
        mPlayer->setPlayerCallback(pCallback);
    }
}

/** 设置文件路径 */
void AVMediaPlayer::setFileName(const char *path){
    if(mPlayer != NULL){
        mPlayer->setFileName(path);
    }
}

/** 开始播放 */
void AVMediaPlayer::play(){
    if(mPlayer != NULL){
        mPlayer->play();
    }
}

/** 暂停播放 */
void AVMediaPlayer::pause(){
    if(mPlayer != NULL){
        mPlayer->pause();
    }
}

/** 继续播放 */
void AVMediaPlayer::restart(){
    if(mPlayer != NULL){
        mPlayer->restart();
    }
}

/** 停止播放 */
void AVMediaPlayer::stop(){
    if(mPlayer != NULL){
        mPlayer->stop();
    }
}

/** 拖动播放 */
void AVMediaPlayer::seek(int ms){
    if(mPlayer != NULL){
        mPlayer->seek(ms);
    }
}

/** 设置音量(0-100),音量默认值为100 */
void AVMediaPlayer::setVolume(int vol){
    if(mPlayer != NULL){
        mPlayer->setVolume(vol);
    }
}

/** 获取当前音量 */
int AVMediaPlayer::getVolume(){
    if(mPlayer != NULL){
        return mPlayer->getVolume();
    }
    return 0;
}

/** 设置播放速率(0.125 - 8)
*   0.125 以1/8的速度播放(慢速播放)
*   8 以8/1的速度播放(快速播放)
*   慢速播放公式(rate = 1.0 / 倍数(最大为8))
*   快速播放公式(rate = 1.0 * 倍数(最大为8))
*/
void AVMediaPlayer::setPlaybackRate(float rate){
    if(mPlayer != NULL){
        mPlayer->setPlaybackRate(rate);
    }
}

/** 获取当前播放速率 */
float AVMediaPlayer::getPlaybackRate(){
    if(mPlayer != NULL){
        return mPlayer->getPlaybackRate();
    }
    return 1;
}

/** 获取当前视频的总时长 */
int AVMediaPlayer::getDuration(){
    if(mPlayer != NULL){
        return mPlayer->getDuration();
    }
    return 0;
}

/** 获取当前视频的播放位置 */
int AVMediaPlayer::getPosition(){
    if(mPlayer != NULL){
        return mPlayer->getPosition();
    }
    return 0;
}

/** 获取当前媒体状态 */
int AVMediaPlayer::getCurrentMediaStatus(){
    if(mPlayer != NULL){
        return mPlayer->getCurrentMediaStatus();
    }
    return (int)AVDefine::MediaStatus_UnknownStatus;
}

/** 获取当前播放状态 */
int AVMediaPlayer::getPlaybackState(){
    if(mPlayer != NULL){
        return mPlayer->getPlaybackState();
    }
    return (int)AVDefine::StoppedState;
}
