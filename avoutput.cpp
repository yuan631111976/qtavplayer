#include "AVOutput.h"
#include <QDebug>
#include <QDateTime>

#include <QOpenGLContext>
#include <QOpenGLFramebufferObjectFormat>
#include <string.h>

//顶点数组(物体表面坐标取值范围是-1到1,数组坐标：左下，右下，左上，右上)
//左上 -1.0f  -1.0f
//右上 1.0f  -1.0f
//左下 -1.0f  1.0f
//右下 1.0f  1.0f
static const GLfloat vertexVertices[] = {
   -1.0f, -1.0f,
   1.0f, -1.0f,
   -1.0f,  1.0f,
   1.0f,  1.0f,
};

//左上 0.0f  1.0f
//右上 1.0f  1.0f
//左下 0.0f  0.0f
//右下 1.0f  0.0f

//像素，纹理数组(纹理坐标取值范围是0-1，坐标原点位于左下角,数组坐标：左上，右上，左下，右下,如果先左下，图像会倒过来)
static const GLfloat textureVertices[] = {
    0.0f,  0.0f,
    1.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  1.0f,
};

AVRenderer::~AVRenderer()
{
//    if(m_buffer != NULL){
//        delete m_buffer;
//    }

    if(m_program != NULL){
        m_program->deleteLater();
        m_program = NULL;
    }

    if(m_vbo != NULL){
        delete m_vbo;
        m_vbo = NULL;
    }

    if(m_ibo != NULL){
        delete m_ibo;
        m_ibo = NULL;
    }

    if(m_vao != NULL){
        m_vao->deleteLater();
        m_vao = NULL;
    }

    glDeleteTextures(TEXTURE_NUMBER, textureId);
}

void AVRenderer::render() {
    paint();
}

QOpenGLFramebufferObject *AVRenderer::createFramebufferObject(const QSize &size) {
    QOpenGLFramebufferObject *fbo = QQuickFramebufferObject::Renderer::createFramebufferObject(size);
//    m_renderFbo = new QOpenGLFramebufferObject(size,fbo->format());
    mForceUpdate = true;
    m_output->update();
    return fbo;
}

void AVRenderer::updateVideoFrame(VideoFormat *format){
    if(format == NULL)
        return;

    mDataMutex.lock();
    if(m_format.width != format->width || m_format.height != format->height || m_format.format != format->format){
        m_format.format = format->format;
        m_format.width = format->width;
        m_format.height = format->height;
        m_format.rotate = format->rotate;
        mIsNeedNewUpdate = true;
    }


    m_format.renderFrame = format->renderFrame;
    m_format.renderFrameMutex = format->renderFrameMutex;
    mForceUpdate = true;
    mDataMutex.unlock();
}

void AVRenderer::init(){
    if(m_format.width <= 0 || m_format.height <= 0)
        return;
    if (!m_program) {
        m_program = new QOpenGLShaderProgram;
        switch (m_format.format) {
        case AV_PIX_FMT_YUV420P:mTextureFormatValue = YUV;mRenderFormet = YUV420P;break;
        case AV_PIX_FMT_YUVJ420P:mTextureFormatValue = YUVJ;mRenderFormet = YUV420P;break;
        case AV_PIX_FMT_YUV422P:mTextureFormatValue = YUV;mRenderFormet = YUV422P;break;
        case AV_PIX_FMT_YUVJ422P:mTextureFormatValue = YUVJ;mRenderFormet = YUV422P;break;
        case AV_PIX_FMT_YUVJ444P:mTextureFormatValue = YUV;mRenderFormet = YUV444P;break;
        case AV_PIX_FMT_YUV444P:mTextureFormatValue = YUVJ;mRenderFormet = YUV444P;break;
        case AV_PIX_FMT_GRAY8:mTextureFormatValue = GRAY;mRenderFormet = GRAY8;break;
        case AV_PIX_FMT_UYVY422:mTextureFormatValue = YUV;mRenderFormet = YUV422;break;
        case AV_PIX_FMT_YUV420P10LE:mTextureFormatValue = YUV;mRenderFormet = YUV420P;mBitDepth = 10;break;

        default :mTextureFormatValue = YUV;mRenderFormet = YUV420P;break;
        }


        m_program->addShaderFromSourceFile(QOpenGLShader::Vertex,":/video.vert");
        if(mRenderFormet == YUV420 || mRenderFormet == YUV422 || mRenderFormet == YUV444){
            m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,":/yuv-packed.frag");
        }else{
            m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,":/yuv-planar.frag");
        }

        m_program->link();
        mVertexInLocaltion = m_program->attributeLocation("vertexIn");
        mTextureInLocaltion = m_program->attributeLocation("textureIn");

        matrix = m_program->uniformLocation("matrix");
        textureLocaltion[0] = m_program->uniformLocation("tex_y");
        textureLocaltion[1] = m_program->uniformLocation("tex_u");
        textureLocaltion[2] = m_program->uniformLocation("tex_v");

        mAlpha = m_program->uniformLocation("alpha");
        mTextureFormat = m_program->uniformLocation("tex_format");
        mTextureOffset = m_program->uniformLocation("tex_offset");
    }

    if(!mIsInitTextures){
        //初使化纹理
        glGenTextures(TEXTURE_NUMBER, textureId);
        for(int i = 0; i < TEXTURE_NUMBER; i++)
        {
            glBindTexture(GL_TEXTURE_2D, textureId[i]);
            if(i == 0 || mRenderFormet == YUV444P){
                glTexImage2D ( GL_TEXTURE_2D, 0, GL_LUMINANCE, m_format.renderFrame->linesize[i],m_format.renderFrame->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
            }else{
                glTexImage2D ( GL_TEXTURE_2D, 0, GL_LUMINANCE, m_format.renderFrame->linesize[i],mRenderFormet == YUV422P ? m_format.renderFrame->height : m_format.renderFrame->height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
            }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
    mIsInitTextures = true;

    if(!mIsInitPbo){
        for(int i = 0;i < 1;i++){
            for(int j = 0;j < TEXTURE_NUMBER;j++){
                m_pbo[i][j] = QOpenGLBuffer(QOpenGLBuffer::PixelUnpackBuffer);
                m_pbo[i][j].setUsagePattern(QOpenGLBuffer::StreamDraw);
                m_pbo[i][j].create();
            }
        }
        mIsInitPbo = true;
    }

    if(!m_vbo){
        m_vbo = new QOpenGLBuffer;
        m_vbo->setUsagePattern( QOpenGLBuffer::StaticDraw );
        m_vbo->create( );
        m_vbo->bind( );
        m_vbo->allocate( vertexVertices, sizeof( vertexVertices ));
        m_vbo->release();
    }

    if(!m_ibo){
        m_ibo = new QOpenGLBuffer;
        m_ibo->setUsagePattern( QOpenGLBuffer::StaticDraw );
        m_ibo->create( );
        m_ibo->bind( );
        m_ibo->allocate( textureVertices, sizeof( textureVertices ));
        m_ibo->release();
    }

    if(!m_vao){
        m_vao = new QOpenGLVertexArrayObject;
        m_vao->create();
        m_vao->bind();

        m_vbo->bind();
        m_program->setAttributeBuffer( mVertexInLocaltion, GL_FLOAT, 0, 2 );
        m_program->enableAttributeArray(mVertexInLocaltion);
        m_vbo->release();

        m_ibo->bind();
        m_program->setAttributeBuffer( mTextureInLocaltion, GL_FLOAT, 0, 2 );
        m_program->enableAttributeArray(mTextureInLocaltion);
        m_ibo->release();

        m_vao->release();
    }
}
#include <QtEndian>
void AVRenderer::paint(){
    if(m_format.width <= 0 || m_format.height <= 0)
        return;

    //计算真实的渲染FPS
    if(!mTime.isValid())
        mTime.start();

    int elapsed = mTime.elapsed();
    if(elapsed - mLastTime >= 1000){ //1秒钟
        mLastTime = 0;
        mTime.restart();
        m_output->setReallyFps(mFps);
        mFps = 0;
    }
    //end 计算真实的渲染FPS

    if(!mForceUpdate){
        return;
    }

    ++mFps;

    mDataMutex.lock();
    m_format.renderFrameMutex->lock();

    if(m_format.renderFrame == NULL ||
            m_format.renderFrame->data[0] == NULL ||
            m_format.renderFrame->width <= 0 ||
            m_format.renderFrame->height <= 0){
        m_format.renderFrameMutex->unlock();
        mDataMutex.unlock();
        return;
    }

    if(mIsNeedNewUpdate){
        if(m_program != NULL){
            m_program->deleteLater();
            m_program = NULL;
        }

        if(m_vbo != NULL){
            delete m_vbo;
            m_vbo = NULL;
        }

        if(m_ibo != NULL){
            delete m_ibo;
            m_ibo = NULL;
        }

        if(m_vao != NULL){
            m_vao->deleteLater();
            m_vao = NULL;
        }

        if(mIsInitTextures)
            glDeleteTextures(TEXTURE_NUMBER, textureId);
        mIsInitTextures = false;

        init();

        mIsNeedNewUpdate = false;
    }

    QOpenGLFramebufferObject *displayFbo = framebufferObject();

    displayFbo->bind();
    m_program->bind();

    pboIndex = 0;


    qint64 textureSize = m_format.renderFrame->linesize[0]*m_format.renderFrame->height;
    for(int j = 0;j < TEXTURE_NUMBER;j++){
        m_pbo[pboIndex][j].bind();
        glActiveTexture(GL_TEXTURE0 + j);
        glBindTexture(GL_TEXTURE_2D, textureId[j]);

        int linesize = m_format.renderFrame->linesize[j];
        uint8_t * data = m_format.renderFrame->data[j];
//        qDebug() << "----------------------- : " << linesize << ":" << m_format.width << m_format.height;
        if(data != NULL && linesize != 0){
            if(j == 0 || mRenderFormet == YUV444P){//y (444的UV数据和Y一样多)
                if(m_pbo[pboIndex][j].size() != textureSize)
                    m_pbo[pboIndex][j].allocate(textureSize);
                m_pbo[pboIndex][j].write(0,data , textureSize);
                glTexSubImage2D(GL_TEXTURE_2D,0,0,0, linesize,m_format.renderFrame->height, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
                m_program->setUniformValue(mTextureOffset, (GLfloat)((linesize - m_format.renderFrame->width) * 1.0 / linesize)); //偏移量
            }else{//uv
                int uvsize = mRenderFormet == YUV422P ? textureSize / 2 : textureSize / 4;
                if(m_pbo[pboIndex][j].size() != uvsize)
                    m_pbo[pboIndex][j].allocate(uvsize);
                m_pbo[pboIndex][j].write(0,data , uvsize);
                glTexSubImage2D(GL_TEXTURE_2D,0,0,0, linesize,mRenderFormet == YUV422P ? m_format.renderFrame->height : m_format.renderFrame->height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
            }
        }
        m_program->setUniformValue(textureLocaltion[j], j);
        m_pbo[pboIndex][j].release();
    }

    m_format.renderFrameMutex->unlock();
    mDataMutex.unlock();


    int rotate = m_format.rotate;
    switch (m_output->orientation()) {
    case AVOutput::LandscapeOrientation:rotate += 90;break;
    case AVOutput::InvertedLandscapeOrientation:rotate += 270;break;
    case AVOutput::InvertedPortraitOrientation:rotate += 180;break;
    }
    rotate %= 360;

    QMatrix4x4 modelview;
    modelview.rotate(rotate, 0.0f, 0.0f, 1.0f);
    m_program->setUniformValue(matrix,modelview);
    m_program->setUniformValue(mTextureFormat, (GLfloat)mTextureFormatValue); //纹理格式(YUV,YUVJ,RGB)
    m_program->setUniformValue(mAlpha, (GLfloat)1.0); //透明度


    QColor color = m_output->backgroundColor();
    glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    glClear(GL_COLOR_BUFFER_BIT);

    m_displayRect = m_output->calculateGeometry(rotate == 90 || rotate == 270 ?
                                                m_format.height : m_format.width,
                                                rotate == 90 || rotate == 270 ?
                                                m_format.width : m_format.height);

    glViewport(m_displayRect.x(),m_displayRect.y(),m_displayRect.width(),m_displayRect.height());

    m_vao->bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_vao->release();
    m_program->release();
    displayFbo->bindDefault();
    m_output->window()->resetOpenGLState(); //重置opengl状态，不然界面上的文字会出现花屏

//    QOpenGLFramebufferObject *displayFbo = framebufferObject();
//    qSwap(m_renderFbo,displayFbo);

    mForceUpdate = false;
}

AVOutput::AVOutput(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
    , mPlayer(NULL)
    , mFillMode(PreserveAspectFit)
    , mOrientation(PrimaryOrientation)
    , mFps(30)
    , mIsDestroy(false)
    , mBackgroundColor(QColor(255,255,0,255))
    , mReallyFps(0)
{
//    setFlag(ItemHasContents);
    connect(&mTimer,SIGNAL(timeout()),this,SLOT(update()));
    mTimer.setInterval(1000 / mFps);
    mTimer.start();
}
AVOutput::~AVOutput(){
    mIsDestroy = true;
}

QObject *AVOutput::source() const{
    return mPlayer;
}
void AVOutput::setSource(QObject *source){
    if(mPlayer){
        disconnect(mPlayer,SIGNAL(updateVideoFrame(const char*,VideoFormat*)),
                this,SIGNAL(updateVideoFrame(const char*,VideoFormat*)));
        disconnect(mPlayer,SIGNAL(playStatusChanged()),this,SLOT(playStatusChanged()));
    }
    if(mPlayer == source)
        return;
    mPlayer = (AVPlayer *)source;

    connect(mPlayer,SIGNAL(updateVideoFrame(VideoFormat*)),
            this,SIGNAL(updateVideoFrame(VideoFormat*)),Qt::DirectConnection);

    connect(mPlayer,SIGNAL(playStatusChanged()),this,SLOT(playStatusChanged()));

    emit sourceChanged();
}

AVOutput::FillMode AVOutput::fillMode() const{
    return mFillMode;
}
void AVOutput::setFillMode(FillMode mode){
    mFillMode = mode;
    fillModeChanged();
}
int AVOutput::fps() const{
    return mFps;
}

void AVOutput::setFps(int fps){
    mFps = fps;
    mTimer.setInterval(1000 / mFps);
    mTimer.start();
}

/** 真实的fps */
int AVOutput::reallyFps() const{
    return mReallyFps;
}

void  AVOutput::setReallyFps(int reallyFps){
    this->mReallyFps = reallyFps;
    emit reallyFpsChanged();
}

QColor AVOutput::backgroundColor() const{
    return mBackgroundColor;
}
void AVOutput::setBackgroundColor(QColor &color){
    this->mBackgroundColor = color;
    emit backgroundColorChanged();
}

AVOutput::Orientation AVOutput::orientation() const{
    return mOrientation;
}
void AVOutput::setOrientation(Orientation orientation){
    this->mOrientation = orientation;
    emit orientationChanged();
}

QRect AVOutput::calculateGeometry(int w,int h){
    switch(mFillMode){
        case AVOutput::Stretch : {
            return QRect(0,0,width(),height());
        }
        case AVOutput::PreserveAspectCrop :
        case AVOutput::PreserveAspectFit :
        default :{
            float rate1 = w / width();
            float rate2 = h / height();
            float rate = rate1 > rate2 ? rate1 : rate2;
            if(mFillMode == AVOutput::PreserveAspectCrop){
                rate = rate1 < rate2 ? rate1 : rate2;
            }
            float newWidth = w / rate;
            float newHeight = h / rate;
            return QRect((width() - newWidth ) / 2,(height() - newHeight) / 2,newWidth,newHeight);
        }
    }
}

QQuickFramebufferObject::Renderer *AVOutput::createRenderer() const{
    AVRenderer *renderer = new AVRenderer((AVOutput *)this);
    connect(this,SIGNAL(updateVideoFrame(VideoFormat*)),
            renderer,SLOT(updateVideoFrame(VideoFormat*)),Qt::DirectConnection);

    renderer->updateVideoFrame(mPlayer->getRenderData());
    return renderer;
}

void AVOutput::playStatusChanged(){
    if(mPlayer != NULL){
        if(mPlayer->getPlaybackState() == AVDefine::PlayingState){
            mTimer.start();
        }else{
            mTimer.stop();
            setReallyFps(0);
        }
    }
}
