#ifndef AVDEFINE_H
#define AVDEFINE_H

#define GENERATE_QML_PROPERTY(Name,Type) Q_PROPERTY(Type Name READ get##Name WRITE set##Name NOTIFY Name##Changed)
#define GENERATE_QML_NOSET_PROPERTY(Name,Type) Q_PROPERTY(Type Name READ get##Name NOTIFY Name##Changed)

#define GENERATE_GET_SET_PROPERTY_CHANGED_NO_IMPL(Name,Type) \
    public : \
        Type m##Name; \
        Type get##Name() const; \
        void set##Name(Type value); \
    Q_SIGNALS : \
        void Name##Changed();

// 生成属性，GET()及实现，SET不带实现,和change信号
#define GENERATE_GET_SET_PROPERTY_CHANGED(Name,Type) \
    private : \
        Type m##Name; \
        Type get##Name() const{return m##Name;} \
        void set##Name(Type value); \
    Q_SIGNALS : \
        void Name##Changed();

#define GENERATE_GET_SET_PROPERTY_CHANGED_IMPL_SET(Name,Type) \
    private : \
        Type m##Name; \
    public : \
        Type get##Name() const{return m##Name;} \
        void set##Name(Type value){m##Name = value;} \
    Q_SIGNALS : \
        void Name##Changed();

#define GENERATE_GET_PROPERTY_CHANGED(Name,Type) \
    private : \
        Type m##Name; \
        Type get##Name() const{return m##Name;} \
    Q_SIGNALS : \
        void Name##Changed();

#define GENERATE_GET_PROPERTY_CHANGED_NO_IMPL(Name,Type) \
    public : \
        Type m##Name; \
        Type get##Name() const; \
    Q_SIGNALS : \
        void Name##Changed();

#include <QObject>
class AVDefine : public QObject{
    Q_OBJECT
    Q_ENUMS(AVMediaStatus)
    Q_ENUMS(AVMediaBufferMode)
    Q_ENUMS(AVPlayState)
    Q_ENUMS(AVMediaError)
    Q_ENUMS(AVChannelLayout)
    Q_ENUMS(AVSynchMode)
    Q_ENUMS(AVFillMode)
    Q_ENUMS(AVOrientation)
    Q_ENUMS(AVPlaySpeedRate)
    Q_ENUMS(AVKDMode)
    Q_ENUMS(AVDecodeMode)

public :
    /**
 * @brief The MediaStatus enum
 * 多媒体执行状态
 */
    enum AVMediaStatus {
        AVMediaStatus_UnknownStatus , //未知状态
        AVMediaStatus_NoMedia , //未找到多媒体
        AVMediaStatus_Loading , //加载中
        AVMediaStatus_Loaded , //加载完成
        AVMediaStatus_Seeking , //拖动中
        AVMediaStatus_Seeked , //拖动完成
        AVMediaStatus_Stalled ,
        AVMediaStatus_Buffering , //缓冲中
        AVMediaStatus_Buffered , //缓冲完成
        AVMediaStatus_Played, //播放完成
        AVMediaStatus_EndOfMedia ,//己加载到文件结尾
        AVMediaStatus_InvalidMedia // 无效的多媒体
    };

    /**
     * @brief The MediaBufferMode enum
     * 缓冲区模式
     */
    enum AVMediaBufferMode{
        AVMediaBufferMode_Time,//ms 缓冲区最大可缓冲此毫秒数量的音视频包
        AVMediaBufferMode_Packet = AVMediaBufferMode_Time, //
        AVMediaBufferMode_Default = AVMediaBufferMode_Time
    };

    /** 播放状态 */
    enum AVPlayState {
        AVPlayState_Stopped,
        AVPlayState_Playing,
        AVPlayState_Paused ,
        AVPlayState_Default = AVPlayState_Stopped
    };

    /** 媒体错误类型 */
    enum AVMediaError {
        AVMediaError_NoError,
        AVMediaError_ResourceError,
        AVMediaError_FormatError,
        AVMediaError_NetworkError,
        AVMediaError_AccessDenied,
        AVMediaError_ServiceMissing ,
        AVMediaError_Default = AVMediaError_NoError
    };

    /** 声音播放通道枚举 */
    enum AVChannelLayout {
        AVChannelLayout_Auto, //使用默认的
        AVChannelLayout_Left, //左声道
        AVChannelLayout_Right,
        AVChannelLayout_Mono,
        AVChannelLayout_Stereo , //立体声
        AVChannelLayout_Default = AVChannelLayout_Auto
    };

    /** 音视频同步模式 */
    enum AVSynchMode{
        AVSynchMode_Audio_Video, //以音频时间为基准，默认 (有音频时，以音频为基准，无音频时，以视频自身为其准)
        AVSynchMode_Timer = AVSynchMode_Audio_Video, //以系统时间为基准(暂时未实现)
        AVSynchMode_Default = AVSynchMode_Audio_Video
    };

    /** 图像填充模式 */
    enum AVFillMode {
        AVFillMode_Stretch , //填满屏幕 ,不保护比例/the image is scaled to fit
        AVFillMode_PreserveAspectFit, //保护缩放比例/the image is scaled uniformly to fit without cropping
        AVFillMode_PreserveAspectCrop ,//the image is scaled uniformly to fill, cropping if necessary
        AVFillMode_Default = AVFillMode_PreserveAspectFit
    };

    /** 视频显示方向 */
    enum AVOrientation{
        AVOrientation_PrimaryOrientation = Qt::PrimaryOrientation, // default ,无旋转
        AVOrientation_LandscapeOrientation = Qt::LandscapeOrientation, // 旋转90度
        AVOrientation_PortraitOrientation = Qt::PortraitOrientation, // 无旋转
        AVOrientation_InvertedLandscapeOrientation = Qt::InvertedLandscapeOrientation, // 旋转270度
        AVOrientation_InvertedPortraitOrientation = Qt::InvertedPortraitOrientation ,// 旋转180度
        AVOrientation_Default = AVOrientation_PrimaryOrientation
    };

    /** 播放速率 */
    enum AVPlaySpeedRate{
        AVPlaySpeedRate_Normal, //正常的播放速率
        AVPlaySpeedRate_Q1_5, //1.5倍的速度
        AVPlaySpeedRate_Q2, //2倍速度
        AVPlaySpeedRate_Q4, //4倍速度
        AVPlaySpeedRate_Q8, //8倍速度
        AVPlaySpeedRate_S1_5, //四分之三的速度
        AVPlaySpeedRate_S2 , //二分之一的速度
        AVPlaySpeedRate_S4, //四分之一的速度
        AVPlaySpeedRate_S8, //八分之一的速度
        AVPlaySpeedRate_Default = AVPlaySpeedRate_Normal
    };

    /** 卡顿模式 (当视频解码处理不过来的时候，先择的方法) */
    enum AVKDMode {
        AVKDMode_Audio_Wait , // 暂停音频，等待视频解码 ,达到同步的目的
        AVKDMode_ThrowAway_Video , // 丢掉一部份视频帧，使视频解码跟上音频，达到同步的目的 (会出现花屏的现象) (倍速播放时，若视频解码跟不上时，强制使用些模式)
        AVKDMode_Default = AVKDMode_ThrowAway_Video
    };

    /** 解码模式 */
    enum AVDecodeMode {
        AVDecodeMode_Soft , // 软解
        AVDecodeMode_VDPAU , //
        AVDecodeMode_CUDA , //
        AVDecodeMode_VAAPI , //
        AVDecodeMode_DXVA2 , //
        AVDecodeMode_QSV , //
        AVDecodeMode_VIDEOTOOLBOX , //
        AVDecodeMode_D3D11VA , //
        AVDecodeMode_DRM , //
        AVDecodeMode_OPENCL ,
        AVDecodeMode_MEDIACODEC , //
        AVDecodeMode_HW , //(硬解) 如果想在初使化的时候,不知道有哪些硬解类型，又想使用硬解，则设置此值（程序会自动选择硬解方式，如果不支持，则还是软解）

        AVDecodeMode_Default =  AVDecodeMode_Soft
    };
};
#endif // AVDEFINE_H
