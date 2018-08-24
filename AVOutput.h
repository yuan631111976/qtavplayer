#ifndef AVOUTPUT_H
#define AVOUTPUT_H


#include <QQuickFramebufferObject>
#include <QMutex>

#include "AVPlayer.h"
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOffscreenSurface>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QDateTime>
#include <QQuickWindow>
#include <QTimer>

enum TextureFormat{
    YUV = 0,
    YUVJ = 1,
    RGB = 2,
    GRAY = 3,
    YUYV = 4,
    UYVY = 5 ,
    BGR = 6,
    UYYVYY = 7,
    NV12 = 8 ,
    NV21 = 9 ,
    YUV420P10LE = 10,
    BGR8 = 11,
    RGBA = 12 ,
    ARGB = 13 ,
    ABGR = 14 ,
    BGRA = 15 ,
    YUV16 = 16,
    YUVJ16 = 17,
    BGR4 = 18,
    BGGR = 19,
    RGGB = 20 ,
    GRBG = 21 ,
    GBRG = 22 ,
};

class RenderParams{
public :
    RenderParams(AVPixelFormat f,AVRational yw,AVRational uw,AVRational vw,AVRational yh,AVRational uh,AVRational vh,AVRational y,AVRational u,AVRational v,GLint yf1,GLenum yf2,GLint uf1,GLenum uf2,GLint vf1,GLenum vf2,int f3,bool planar,GLenum dataType = GL_UNSIGNED_BYTE)
        : format(f)
        , yWidth(yw)
        , uWidth(uw)
        , vWidth(vw)

        , yHeight(yh)
        , uHeight(uh)
        , vHeight(vh)

        , ySize(y)
        , uSize(u)
        , vSize(v)

        , yInternalformat(yf1)
        , uInternalformat(uf1)
        , vInternalformat(vf1)
        , yGlFormat(yf2)
        , uGlFormat(uf2)
        , vGlFormat(vf2)
        , textureFormat(f3)

        , isPlanar(planar)
        , dataType(dataType)
    {

        yuvsizes[0] = ySize;
        yuvsizes[1] = uSize;
        yuvsizes[2] = vSize;

        yuvwidths[0] = yWidth;
        yuvwidths[1] = uWidth;
        yuvwidths[2] = vWidth;

        yuvheights[0] = yHeight;
        yuvheights[1] = uHeight;
        yuvheights[2] = vHeight;

        yuvInternalformat[0] = yInternalformat;
        yuvInternalformat[1] = uInternalformat;
        yuvInternalformat[2] = vInternalformat;

        yuvGlFormat[0] = yGlFormat;
        yuvGlFormat[1] = uGlFormat;
        yuvGlFormat[2] = vGlFormat;
    }

    RenderParams(AVPixelFormat f,AVRational yw,AVRational uw,AVRational vw,AVRational yh,AVRational uh,AVRational vh,AVRational y,AVRational u,AVRational v,GLint f1,GLenum f2,int f3,bool planar,GLenum dataType = GL_UNSIGNED_BYTE): RenderParams(f,yw,uw,vw,yh,uh,vh,y,u,v,f1,f2,f1,f2,f1,f2,f3,planar,dataType){}
    RenderParams():RenderParams(AV_PIX_FMT_YUV420P,{1,1},{1,1},{1,1},{1,1},{1,1},{1,1},{1,1},{1,1},{1,1},0,0,0,false){}
    RenderParams (const RenderParams &r)
        : RenderParams(r.format,
                       r.yWidth,r.uWidth,r.vWidth,
                       r.yHeight,r.uHeight,r.vHeight,
                       r.ySize,r.uSize,r.vSize,
                       r.yInternalformat,r.yGlFormat,
                       r.uInternalformat,r.uGlFormat,
                       r.vInternalformat,r.vGlFormat,
                       r.textureFormat,r.isPlanar,
                       r.dataType)
    {
    }

    AVPixelFormat format;

    AVRational yWidth; //y的宽度(比率)
    AVRational uWidth; //u的宽度(比率)
    AVRational vWidth; //v的宽度(比率)

    AVRational yHeight; //y的高度(比率)
    AVRational uHeight; //u的高度(比率)
    AVRational vHeight; //v的高度(比率)

    AVRational ySize;// y的大小(比率)
    AVRational uSize;// u的大小(比率)
    AVRational vSize;// v的大小(比率)

    GLint yInternalformat; //数据位数
    GLint uInternalformat; //数据位数
    GLint vInternalformat; //数据位数

    GLenum yGlFormat; //
    GLenum uGlFormat; //
    GLenum vGlFormat; //

    int textureFormat; //自定义纹理格式，GL使用

    bool isPlanar; //是否是平面 true : 平面 | false : 打包

    AVRational yuvsizes[3];
    AVRational yuvwidths[3];
    AVRational yuvheights[3];
    GLint yuvInternalformat[3];
    GLenum yuvGlFormat[3];

    GLenum dataType;
};

class AVOutput : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int fillMode READ fillMode WRITE setFillMode NOTIFY fillModeChanged)
    Q_PROPERTY(int fps READ fps WRITE setFps NOTIFY fpsChanged)
    Q_PROPERTY(int reallyFps READ reallyFps WRITE setReallyFps NOTIFY reallyFpsChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(int orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(bool hdrMode READ HDR WRITE setHDR NOTIFY hdrModeChanged)
    Q_PROPERTY(bool vrMode READ VR WRITE setVR NOTIFY vrModeChanged)
    Q_PROPERTY(bool useVideoBackground READ useVideoBackground WRITE setUseVideoBackground NOTIFY useVideoBackgroundChanged)

    Q_ENUMS(FillMode)
    Q_ENUMS(Orientation)
public:
    AVOutput(QQuickItem *parent = 0);
    ~AVOutput();

    int fillMode() const;
    void setFillMode(int mode);

    int fps() const;
    void setFps(int);

    /** 真实的fps */
    int reallyFps() const;
    void setReallyFps(int reallyFps);

    QColor backgroundColor() const;
    void setBackgroundColor(QColor &color);

    int orientation() const;
    void setOrientation(int orientation);

    QObject *source() const;
    void setSource(QObject *source);

    bool HDR();
    void setHDR(bool flag);

    bool VR();
    void setVR(bool flag);

    bool useVideoBackground();
    void setUseVideoBackground(bool flag);

    QRect calculateGeometry(int w,int h);
public slots:
    void playStatusChanged();
protected:
    virtual Renderer *createRenderer() const;

signals :
    void sourceChanged();
    void fillModeChanged();
    void fpsChanged();
    void reallyFpsChanged();
    void backgroundColorChanged();
    void orientationChanged();
    void hdrModeChanged();
    void vrModeChanged();
    void useVideoBackgroundChanged();

    void updateVideoFrame(VideoFormat*);
private :
    friend class AVRenderer;
    AVPlayer *mPlayer;
    AVDefine::FillMode mFillMode;
    AVDefine::Orientation mOrientation;
    QColor mBackgroundColor;
    VideoFormat mFormat;
    QTimer mTimer;
    bool mIsDestroy;
    int mFps;
    int mReallyFps;
    bool mEnableHDR;
    bool mEnableVR;
    bool mUseVideoBackground;
};


class AVRenderer : public QObject , protected QOpenGLFunctions , public QQuickFramebufferObject::Renderer{
    Q_OBJECT
public:
    AVRenderer(AVOutput *output)
        : m_program(NULL)
        , m_renderFbo(NULL)
        , m_vbo(NULL)
        , m_vao(NULL)
        , m_output(NULL)
        , m_ibo(NULL)
        , pboIndex(0)
        , mLastTime(0)
        , mFps(0)
        , mIsInitPbo(false)
        , mIsNeedNewUpdate(false)
        , mIsInitTextures(false)
        , mForceUpdate(true)
        , mLastOffset(0.0f)
    {
//        m_format.width = 0;
//        m_format.height = 0;
//        m_format.format = -1;
//        m_format.mutex = NULL;
//        m_format.rotate = 0;
        memset(&m_format,0,sizeof(m_format));
        m_output = output;
        initializeOpenGLFunctions();
    }
    ~AVRenderer();
public slots :
    void updateVideoFrame(VideoFormat*);

    void paint();
    void init();
public:
    virtual void render();
    virtual QOpenGLFramebufferObject *createFramebufferObject(const QSize &size);
private:
    static const int TEXTURE_NUMBER = 3;
    QOpenGLBuffer m_pbo[2][TEXTURE_NUMBER];
    QOpenGLVertexArrayObject *m_vao;
    QOpenGLBuffer *m_vbo;
    QOpenGLBuffer *m_ibo;
    QOpenGLFramebufferObject *m_renderFbo;
    QOpenGLShaderProgram *m_program;
    QSize yuvSize[TEXTURE_NUMBER];
    qint64 yuvBufferSize[TEXTURE_NUMBER];
    GLuint textureId[TEXTURE_NUMBER];


    int matrix;
    int mVertexInLocaltion;
    int mTextureInLocaltion;
    int textureLocaltion[TEXTURE_NUMBER];
    int mAlpha;
    int mTextureFormat;
    int mTextureFormatValue;
    int mTextureOffset;
    int mImageWidthId;
    int mImageHeightId;
    int mEnableHDRId;
    int enableGaussianBlurId;


    int pboIndex;
    VideoFormat m_format;
    QMutex mDataMutex;
    AVOutput *m_output;
    QRect m_displayRect;

    QTime mTime;
    int mLastTime;
    int mFps;
    bool mIsInitPbo;
    bool mIsNeedNewUpdate;
    bool mIsInitTextures;

    bool mForceUpdate; //强制更新

    RenderParams params;
    GLfloat mLastOffset; //上一次的偏移量
};
#endif // AVOUTPUT_H
