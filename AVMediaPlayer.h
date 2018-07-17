#ifndef AVMEDIAPLAYER_H
#define AVMEDIAPLAYER_H
/**
 * @brief The AVMediaPlayer class
 * 多媒体播放组件
 */

#include "AVDefine.h"
#include "AVPlayerCallback.h"

class AVMediaPlayer
{
public:
    AVMediaPlayer();
    virtual ~AVMediaPlayer();
    /** 删除播放对象，delete当前对象时，必须调用此方法，否则播放对象不能释放 */
    virtual void del();
    /** 设置播放回调接口 */
    virtual void setPlayerCallback(AVPlayerCallback *);
    /** 设置文件路径 */
    virtual void setFileName(const char *path);
    /** 开始播放 */
    virtual void play();
    /** 暂停播放 */
    virtual void pause();
    /** 继续播放 */
    virtual void restart();
    /** 停止播放 */
    virtual void stop();
    /** 拖动播放 */
    virtual void seek(int ms);
    /** 设置音量(0-100),音量默认值为100 */
    virtual void setVolume(int vol);
    /** 获取当前音量 */
    virtual int getVolume();
    /** 设置播放速率(0.125 - 8)
    *   0.125 以1/8的速度播放(慢速播放)
    *   8 以8/1的速度播放(快速播放)
    *   慢速播放公式(rate = 1.0 / 倍数(最大为8))
    *   快速播放公式(rate = 1.0 * 倍数(最大为8))
    */
    virtual void setPlaybackRate(float rate);
    /** 获取当前播放速率 */
    virtual float getPlaybackRate();
    /** 获取当前视频的总时长 */
    virtual int getDuration();
    /** 获取当前视频的播放位置 */
    virtual int getPosition();
    /** 获取当前媒体状态 */
    virtual int getCurrentMediaStatus();
    /** 获取当前播放状态 */
    virtual int getPlaybackState();
protected :
    //此构造不使用，也不能删除
    AVMediaPlayer(int){}
private:
    AVMediaPlayer *mPlayer;
};

#endif // AVMEDIAPLAYER_H
