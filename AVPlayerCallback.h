#ifndef AVPLAYERCALLBACK_H
#define AVPLAYERCALLBACK_H

#include "AVDefine.h"

/**
 * @brief The AVPlayerCallback class
 * 多媒体播放回调接口
 */

class AVPlayerCallback{

public :
    virtual ~AVPlayerCallback(){}
    /** 更新视频帧回调 */
    virtual void updateVideoFrame(const char*,float width,float height,float rotate,int format) = 0;
    /** 视频总时长变化改变回调(ms毫秒) */
    virtual void durationChanged(int) = 0;
    /** 视频播放进度回调(ms毫秒) */
    virtual void positionChanged(int) = 0;
    /** 状态回调 */
    virtual void statusChanged(AVDefine::MediaStatus) = 0;
    /** 播放状态状态回调 */
    virtual void playStatusChanged(AVDefine::PlaybackState) = 0;
};


#endif // AVPLAYERCALLBACK_H
