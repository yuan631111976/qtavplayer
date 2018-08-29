#ifndef AVMEDIACALLBACK_H
#define AVMEDIACALLBACK_H

#include "AVDefine.h"
#include <QByteArray>
#include <QAudioFormat>

class AVMediaCallback{

public :
    AVMediaCallback(){}
    virtual ~AVMediaCallback(){}
    /** 更新视频帧回调 */
    virtual void mediaUpdateVideoFrame(void*){}
    /** 更新音频格式回调 */
    virtual void mediaUpdateAudioFormat(const QAudioFormat&){}
    /** 更新音频数据格式 */
    virtual void mediaUpdateAudioFrame(const QByteArray &){}
    /** 视频总时间变化信息 */
    virtual void mediaDurationChanged(int duration){}
    /** 可以渲染第一帧调 */
    virtual void mediaCanRenderFirstFrame(){}
    /** 状态回调 */
    virtual void mediaStatusChanged(AVDefine::AVMediaStatus){}
    /** 存在音频回调 */
    virtual void mediaHasAudioChanged(){}
    /** 存在视频回调 */
    virtual void mediaHasVideoChanged(){}
};


#endif // AVMEDIACALLBACK_H
