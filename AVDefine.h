#ifndef AVDEFINE_H
#define AVDEFINE_H

#include <QObject>
class AVDefine : public QObject{
    Q_OBJECT
    Q_ENUMS(MediaStatus)
    Q_ENUMS(MediaBufferMode)
    Q_ENUMS(PlaybackState)
    Q_ENUMS(MediaError)
    Q_ENUMS(ChannelLayout)
    Q_ENUMS(SynchMode)
    Q_ENUMS(FillMode)
    Q_ENUMS(Orientation)
public :
    /**
 * @brief The MediaStatus enum
 * 多媒体执行状态
 */
    enum MediaStatus {
        MediaStatus_UnknownStatus , //未知状态
        MediaStatus_NoMedia , //未找到多媒体
        MediaStatus_Loading , //加载中
        MediaStatus_Loaded , //加载完成
        MediaStatus_Seeking , //拖动中
        MediaStatus_Seeked , //拖动完成
        MediaStatus_Stalled ,
        MediaStatus_Buffering , //缓冲中
        MediaStatus_Buffered , //缓冲完成
        MediaStatus_Played, //播放完成
        MediaStatus_EndOfMedia ,//己加载到文件结尾
        MediaStatus_InvalidMedia // 无效的多媒体
    };

    /**
 * @brief The MediaBufferMode enum
 * 缓冲区模式
 */
    enum MediaBufferMode{
        MediaBufferMode_Time,//ms
        MediaBufferMode_Packet
    };

    /** 播放状态 */
    enum PlaybackState {
        StoppedState,
        PlayingState,
        PausedState
    };

    /** 媒体错误类型 */
    enum MediaError {
        NoError,
        ResourceError,
        FormatError,
        NetworkError,
        AccessDenied,
        ServiceMissing
    };

    /** 声音播放通道枚举 */
    enum ChannelLayout {
        ChannelLayoutAuto,
        Left,
        Right,
        Mono,
        Stereo
    };

    /** 音视频同步模式 */
    enum SynchMode{
        AUDIO, //以音频时间为基准，默认
        TIMER //以系统时间为基准
    };

    enum FillMode {
        Stretch , //填满屏幕 ,不保护比例/the image is scaled to fit
        PreserveAspectFit, //保护缩放比例/the image is scaled uniformly to fit without cropping
        PreserveAspectCrop //the image is scaled uniformly to fill, cropping if necessary
    };

    enum Orientation{
        PrimaryOrientation = Qt::PrimaryOrientation, // default ,rotation 0
        LandscapeOrientation = Qt::LandscapeOrientation, // rotation 90
        PortraitOrientation = Qt::PortraitOrientation, // rotation 0
        InvertedLandscapeOrientation = Qt::InvertedLandscapeOrientation, // 270
        InvertedPortraitOrientation = Qt::InvertedPortraitOrientation// 180
    };
};
#endif // AVDEFINE_H
