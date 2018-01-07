#ifndef AVOUTPUT_H
#define AVOUTPUT_H


#include <QQuickFramebufferObject>
#include <QMutex>

#include "avplayer.h"
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOffscreenSurface>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QDateTime>
#include <QQuickWindow>

class AVOutput : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(FillMode fillMode READ fillMode WRITE setFillMode NOTIFY fillModeChanged)
    Q_PROPERTY(int fps READ fps WRITE setFps NOTIFY fpsChanged)
    Q_PROPERTY(int reallyFps READ reallyFps WRITE setReallyFps NOTIFY reallyFpsChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)

    Q_ENUMS(FillMode)
    Q_ENUMS(Orientation)
public:
    AVOutput(QQuickItem *parent = 0);
    ~AVOutput();

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

    FillMode fillMode() const;
    void setFillMode(FillMode mode);

    int fps() const;
    void setFps(int);

    /** 真实的fps */
    int reallyFps() const;
    void setReallyFps(int reallyFps);

    QColor backgroundColor() const;
    void setBackgroundColor(QColor &color);

    Orientation orientation() const;
    void setOrientation(Orientation orientation);

    QObject *source() const;
    void setSource(QObject *source);

    QRect calculateGeometry(int,int);
protected:
    virtual Renderer *createRenderer() const;

signals :
    void sourceChanged();
    void fillModeChanged();
    void fpsChanged();
    void reallyFpsChanged();
    void backgroundColorChanged();
    void orientationChanged();

    void updateVideoFrame(const char*,VideoFormat*);
private :
    AVPlayer *m_player;
    FillMode mFillMode;
    Orientation mOrientation;
    QColor mBackgroundColor;
    VideoFormat format;
    QTimer m_timer;
    bool m_isDestroy;
    int m_fps;
    int m_reallyFps;
};


class AVRenderer : public QObject , protected QOpenGLFunctions , public QQuickFramebufferObject::Renderer{
    Q_OBJECT
public:
    AVRenderer(AVOutput *output)
        : m_program(NULL)
        , m_renderFbo(NULL)
        , m_buffer(NULL)
        , m_vbo(NULL)
        , m_vao(NULL)
        , m_output(NULL)
        , m_ibo(NULL)
        , pboIndex(0)
        , mLastTime(0)
        , mFps(0)
        , mIsInitPbo(false)
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
    void updateVideoFrame(const char*,VideoFormat*);

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
    GLuint textureId[TEXTURE_NUMBER];


    int matrix;
    int mVertexInLocaltion;
    int mTextureInLocaltion;
    int textureLocaltion[TEXTURE_NUMBER];
    int pboIndex;

    VideoFormat m_format;
    AVOutput *m_output;
    QRect m_displayRect;
    unsigned char *m_buffer;
    int mBufferSize;
    /** 数据操作锁 */
    QMutex mMutex;
    QTime mTime;
    int mLastTime;
    int mFps;
    bool mIsInitPbo;
};
#endif // AVOUTPUT_H
