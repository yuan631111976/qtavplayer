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
    if(m_buffer != NULL){
        delete m_buffer;
    }

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
    m_renderFbo = new QOpenGLFramebufferObject(size,fbo->format());
    return fbo;
}

void AVRenderer::updateVideoFrame(const char* pBuffer,VideoFormat *format){
    if(format == NULL || pBuffer == NULL)
        return;
    mMutex.lock();


    if(m_format.width != format->width || m_format.height != format->height){
        m_format.format = format->format;
        m_format.width = format->width;
        m_format.height = format->height;
        m_format.rotate = format->rotate;
        m_format.mutex = NULL;
        mIsNeedNewUpdate = true;
    }

    int newBufferSize = format->width * format->height * 3 / 2;
    if(mIsNeedNewUpdate){
        if(m_buffer != NULL){
            delete m_buffer;
        }
        m_buffer = new unsigned char[newBufferSize];
    }

    memcpy(m_buffer,pBuffer,newBufferSize);
    mMutex.unlock();
}

void AVRenderer::init(){
    if(m_format.width <= 0 || m_format.height <= 0)
        return;

    if (!m_program) {
        m_program = new QOpenGLShaderProgram;
        switch (m_format.format) {
        case AV_PIX_FMT_YUV420P:
            m_program->addShaderFromSourceFile(QOpenGLShader::Vertex,":/yuv420.vert");
            m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,":/yuv420.frag");
            break;
        default:
            m_program->addShaderFromSourceFile(QOpenGLShader::Vertex,":/yuv420.vert");
            m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,":/yuv420.frag");
            break;
        }
        m_program->link();
        mVertexInLocaltion = m_program->attributeLocation("vertexIn");
        mTextureInLocaltion = m_program->attributeLocation("textureIn");

        matrix = m_program->uniformLocation("matrix");
        textureLocaltion[0] = m_program->uniformLocation("tex_y");
        textureLocaltion[1] = m_program->uniformLocation("tex_u");
        textureLocaltion[2] = m_program->uniformLocation("tex_v");
    }

    //初使化纹理
    glGenTextures(TEXTURE_NUMBER, textureId);
    for(int i = 0; i < TEXTURE_NUMBER; i++)
    {
        glBindTexture(GL_TEXTURE_2D, textureId[i]);
        if(i == 0){
            glTexImage2D ( GL_TEXTURE_2D, 0, GL_LUMINANCE, m_format.width,m_format.height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
        }else{
            glTexImage2D ( GL_TEXTURE_2D, 0, GL_LUMINANCE, m_format.width / 2,m_format.height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if(!mIsInitPbo){
        for(int i = 0;i < 2;i++){
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

void AVRenderer::paint(){
    if(m_format.width <= 0 || m_format.height <= 0)
        return;

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

        glDeleteTextures(TEXTURE_NUMBER, textureId);

        init();

        mIsNeedNewUpdate = false;
    }

    m_renderFbo->bind();
    m_program->bind();

    pboIndex = (pboIndex + 1) % 2;
    int nextPboIndex = (pboIndex + 1) % 2;
    qint64 textureSize = m_format.width*m_format.height;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_renderFbo->texture());
    for(int j = 0;j < TEXTURE_NUMBER;j++){
        glActiveTexture(GL_TEXTURE1 + j);
        m_pbo[pboIndex][j].bind();
        if(j == 0){//y
            if(m_pbo[pboIndex][j].size() != textureSize)
                m_pbo[pboIndex][j].allocate(textureSize);
            m_pbo[pboIndex][j].write(0,m_buffer, textureSize);
            glBindTexture(GL_TEXTURE_2D, textureId[j]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0, m_format.width,m_format.height, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
        }else{//uv
            int uvsize = textureSize / 4;
            if(m_pbo[pboIndex][j].size() != uvsize)
                m_pbo[pboIndex][j].allocate(uvsize);
            if(j == 1){
                m_pbo[pboIndex][j].write(0,m_buffer+textureSize, uvsize);
            }else{
                m_pbo[pboIndex][j].write(0,m_buffer+(textureSize+uvsize), uvsize);
            }
            glBindTexture(GL_TEXTURE_2D, textureId[j]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0, m_format.width / 2,m_format.height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
        }
        m_pbo[pboIndex][j].release();
    }

    for(int j = 0;j < TEXTURE_NUMBER;j++){
        m_pbo[nextPboIndex][j].bind();
        if(j == 0){
            glBindTexture(GL_TEXTURE_2D, textureId[j]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0, m_format.width,m_format.height, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
        }else{
            glBindTexture(GL_TEXTURE_2D, textureId[j]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0, m_format.width / 2,m_format.height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
        }
        m_pbo[nextPboIndex][j].release();
    }

    int rotate = m_format.rotate;
    switch (m_output->orientation()) {
    case AVOutput::LandscapeOrientation:
        rotate += 90;
        break;
    case AVOutput::InvertedLandscapeOrientation:
        rotate += 270;
        break;
    case AVOutput::InvertedPortraitOrientation:
        rotate += 180;
        break;
    }

    rotate %= 360;

    QMatrix4x4 modelview;
    modelview.rotate(rotate, 0.0f, 0.0f, 1.0f);
    m_program->setUniformValue(matrix,modelview);
    for(int i = 0;i < TEXTURE_NUMBER;i++){
        glBindTexture(GL_TEXTURE_2D, textureId[i]);
        m_program->setUniformValue(textureLocaltion[i], i+1);
    }

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
    m_renderFbo->bindDefault();
    m_output->window()->resetOpenGLState(); //重置opengl状态，不然界面上的文字会出现花屏

    QOpenGLFramebufferObject *displayFbo = framebufferObject();
    qSwap(m_renderFbo,displayFbo);

    ++mFps;

    if(!mTime.isValid())
        mTime.start();

    int elapsed = mTime.elapsed();
    if(elapsed - mLastTime >= 1000){ //1秒钟
        mLastTime = 0;
        mTime.restart();
        m_output->setReallyFps(mFps);
        mFps = 0;
    }
}

AVOutput::AVOutput(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
    , m_player(NULL)
    , mFillMode(PreserveAspectFit)
    , mOrientation(PrimaryOrientation)
    , m_fps(30)
    , m_isDestroy(false)
    , mBackgroundColor(QColor(255,255,0,255))
    , m_reallyFps(0)
{
//    setFlag(ItemHasContents);
    connect(&m_timer,SIGNAL(timeout()),this,SLOT(update()));
    m_timer.setInterval(1000 / m_fps);
    m_timer.start();
}
AVOutput::~AVOutput(){
    m_isDestroy = true;
}

QObject *AVOutput::source() const{
    return m_player;
}
void AVOutput::setSource(QObject *source){
    if(m_player){
        disconnect(m_player,SIGNAL(updateVideoFrame(const char*,VideoFormat*)),
                this,SIGNAL(updateVideoFrame(const char*,VideoFormat*)));
    }
    if(m_player == source)
        return;
    m_player = (AVPlayer *)source;

    connect(m_player,SIGNAL(updateVideoFrame(const char*,VideoFormat*)),
            this,SIGNAL(updateVideoFrame(const char*,VideoFormat*)),Qt::DirectConnection);
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
    return m_fps;
}

void AVOutput::setFps(int fps){
    m_fps = fps;
    m_timer.setInterval(1000 / m_fps);
}

/** 真实的fps */
int AVOutput::reallyFps() const{
    return m_reallyFps;
}

void  AVOutput::setReallyFps(int reallyFps){
    this->m_reallyFps = reallyFps;
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
    connect(this,SIGNAL(updateVideoFrame(const char*,VideoFormat*)),
            renderer,SLOT(updateVideoFrame(const char*,VideoFormat*)),Qt::DirectConnection);
    return renderer;
}
