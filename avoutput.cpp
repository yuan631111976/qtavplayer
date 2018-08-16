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

static QMap<AVPixelFormat,RenderParams> initRenderParams(){
    QMap<AVPixelFormat,RenderParams> params ;
    AVRational one = {1,1};
    AVRational two = {1,2};
    AVRational three = {1,3};
    AVRational four = {1,4};

    params[AV_PIX_FMT_YUV420P] = RenderParams(AV_PIX_FMT_YUV420P,one,one,one,one,two,two,one,two,two,GL_LUMINANCE,GL_LUMINANCE,YUV,true);
    params[AV_PIX_FMT_YUVJ420P] = RenderParams(AV_PIX_FMT_YUVJ420P,one,one,one,one,two,two,one,two,two,GL_LUMINANCE,GL_LUMINANCE,YUVJ,true);
    params[AV_PIX_FMT_YUV422P] = RenderParams(AV_PIX_FMT_YUV422P,one,one,one,one,one,one,one,one,one,GL_LUMINANCE,GL_LUMINANCE,YUV,true);
    params[AV_PIX_FMT_YUVJ422P] = RenderParams(AV_PIX_FMT_YUVJ422P,one,one,one,one,one,one,one,one,one,GL_LUMINANCE,GL_LUMINANCE,YUVJ,true);
    params[AV_PIX_FMT_YUV444P] = RenderParams(AV_PIX_FMT_YUV444P,one,one,one,one,one,one,one,one,one,GL_LUMINANCE,GL_LUMINANCE,YUV,true);
    params[AV_PIX_FMT_YUVJ444P] = RenderParams(AV_PIX_FMT_YUVJ444P,one,one,one,one,one,one,one,one,one,GL_LUMINANCE,GL_LUMINANCE,YUVJ,true);
    params[AV_PIX_FMT_GRAY8] = RenderParams(AV_PIX_FMT_GRAY8,one,one,one,one,one,one,one,one,one,GL_LUMINANCE,GL_LUMINANCE,GRAY,true);
    params[AV_PIX_FMT_UYVY422] = RenderParams(AV_PIX_FMT_UYVY422,four,one,one,one,one,one,one,one,one,GL_RGBA,GL_RGBA,UYVY,false);
    params[AV_PIX_FMT_YUYV422] = RenderParams(AV_PIX_FMT_YUYV422,four,one,one,one,one,one,one,one,one,GL_RGBA,GL_RGBA,YUYV,false);
    params[AV_PIX_FMT_BGR24] = RenderParams(AV_PIX_FMT_BGR24,three,one,one,one,one,one,one,one,one,GL_RGB8,GL_BGR,BGR,true);
    params[AV_PIX_FMT_RGB24] = RenderParams(AV_PIX_FMT_RGB24,three,one,one,one,one,one,one,one,one,GL_RGB8,GL_RGB,RGB,true);
    params[AV_PIX_FMT_YUV410P] = RenderParams(AV_PIX_FMT_YUV410P,one,four,four,one,four,four,one,four,four,GL_LUMINANCE,GL_LUMINANCE,YUV,true);
    params[AV_PIX_FMT_YUV411P] = RenderParams(AV_PIX_FMT_YUV411P,one,two,two,one,two,two,one,two,two,GL_LUMINANCE,GL_LUMINANCE,YUV,true);
    params[AV_PIX_FMT_MONOWHITE] = RenderParams(AV_PIX_FMT_MONOWHITE,one,one,one,one,one,one,one,one,one,GL_RGB8,GL_LUMINANCE,RGB,true);
    params[AV_PIX_FMT_MONOBLACK] = RenderParams(AV_PIX_FMT_MONOBLACK,one,one,one,one,one,one,one,one,one,GL_RGB8,GL_LUMINANCE,RGB,true);
    params[AV_PIX_FMT_PAL8] = RenderParams(AV_PIX_FMT_PAL8,one,one,one,one,one,one,one,two,two,GL_LUMINANCE,GL_LUMINANCE,RGB,true);

    params[AV_PIX_FMT_UYYVYY411] = RenderParams(AV_PIX_FMT_UYYVYY411,one,two,two,one,two,two,one,two,two,GL_RGBA,GL_RGBA,UYYVYY,false);

    params[AV_PIX_FMT_BGR8] = RenderParams(AV_PIX_FMT_BGR8,three,one,one,one,one,one,one,one,one,GL_RGB8,GL_BGR,BGR,true);
    params[AV_PIX_FMT_NV12] = RenderParams(AV_PIX_FMT_NV12,one,one,one,one,one,one,one,two,two,GL_LUMINANCE,GL_LUMINANCE,NV12,false);
    params[AV_PIX_FMT_NV21] = RenderParams(AV_PIX_FMT_NV21,one,one,one,one,one,one,one,two,two,GL_LUMINANCE,GL_LUMINANCE,NV21,false);


    params[AV_PIX_FMT_YUV420P10LE] = RenderParams(AV_PIX_FMT_YUV420P10LE,two, two,two,one,two,two,one,two,two,GL_LUMINANCE_ALPHA,GL_LUMINANCE_ALPHA,YUV420P10LE,true);
    params[AV_PIX_FMT_YUV444P10LE] = RenderParams(AV_PIX_FMT_YUV444P10LE,two,two,two,one,one,one,one,one,one,GL_LUMINANCE_ALPHA,GL_LUMINANCE_ALPHA,YUV420P10LE,true);
    return params;
}

static const QMap<AVPixelFormat,RenderParams> renderParams = initRenderParams();


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
        params = renderParams[(AVPixelFormat)m_format.format];

        m_program->addShaderFromSourceFile(QOpenGLShader::Vertex,":/video.vert");
        m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,params.isPlanar ? ":/yuv-planar.frag" : ":/yuv-packed.frag");

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
        mImageWidthId = m_program->uniformLocation("imageWidth");
        mImageHeightId = m_program->uniformLocation("imageHeight");
        mEnableHDRId = m_program->uniformLocation("enableHDR");


    }

    if(!mIsInitTextures){
        //初使化纹理
        glGenTextures(TEXTURE_NUMBER, textureId);
        for(int i = 0; i < TEXTURE_NUMBER; i++)
        {
            glBindTexture(GL_TEXTURE_2D, textureId[i]);
            int linesize = qAbs(m_format.renderFrame->linesize[i]);
            AVRational widthRational = params.yuvwidths[i];
            AVRational heightRational = params.yuvheights[i];
            int width = linesize * widthRational.num / widthRational.den;
            int height = m_format.renderFrame->height * heightRational.num / heightRational.den;
//            qDebug() << width << height;
            glTexImage2D ( GL_TEXTURE_2D, 0, params.internalformat,width ,height, 0, params.glFormat, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
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

    for(int j = 0;j < TEXTURE_NUMBER;j++){
        m_pbo[pboIndex][j].bind();
        glActiveTexture(GL_TEXTURE0 + j);
        glBindTexture(GL_TEXTURE_2D, textureId[j]);

        int linesize = m_format.renderFrame->linesize[j];
        uint8_t * data = m_format.renderFrame->data[j];
        if(data != NULL && linesize != 0){
            qint64 textureSize = qAbs(linesize)*m_format.renderFrame->height;

            textureSize = textureSize * params.yuvsizes[j].num / params.yuvsizes[j].den;

            if(m_pbo[pboIndex][j].size() != textureSize)
                m_pbo[pboIndex][j].allocate(textureSize);

            if(linesize < 0){
                for(int i = 0;i < m_format.renderFrame->height;i++ ){
                    m_pbo[pboIndex][j].write(i * qAbs(linesize),data + i * linesize,qAbs(linesize));
                }
            }else{
                m_pbo[pboIndex][j].write(0,data , textureSize);
            }
            linesize = qAbs(linesize);


//            qDebug() << "----------" << linesize << m_format.renderFrame->width << m_format.renderFrame->height << j;
            if(j == 0){// Y or R
                m_program->setUniformValue(mTextureOffset, (GLfloat)((linesize - m_format.renderFrame->width) * 1.0 / linesize)); //偏移量
            }
            int width = linesize * params.yuvwidths[j].num / params.yuvwidths[j].den;
            int height = m_format.renderFrame->height * params.yuvheights[j].num / params.yuvheights[j].den;
//            qDebug() << width << height;
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height, params.glFormat, GL_UNSIGNED_BYTE, NULL);
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
    m_program->setUniformValue(mTextureFormat, (GLfloat)params.textureFormat); //纹理格式(YUV,YUVJ,RGB)
    m_program->setUniformValue(mAlpha, (GLfloat)1.0); //透明度
    m_program->setUniformValue(mEnableHDRId, m_output->HDR()); //HDR


    m_displayRect = m_output->calculateGeometry(rotate == 90 || rotate == 270 ?
                                                m_format.height : m_format.width,
                                                rotate == 90 || rotate == 270 ?
                                                m_format.width : m_format.height);

    m_program->setUniformValue(mImageWidthId, m_format.width); //图像宽度
    m_program->setUniformValue(mImageHeightId, m_format.height); //图像高度

    QColor color = m_output->backgroundColor();
    glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_MULTISAMPLE);


    m_vao->bind();
    if(m_output->VR()){
        glViewport(m_displayRect.x(),m_displayRect.y(),m_displayRect.width() / 2,m_displayRect.height());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glViewport(m_displayRect.width() / 2,m_displayRect.y(),m_displayRect.width() / 2,m_displayRect.height());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }else{
        glViewport(m_displayRect.x(),m_displayRect.y(),m_displayRect.width(),m_displayRect.height());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
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
    , mEnableHDR(false)
    , mEnableVR(false)
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

bool AVOutput::HDR(){
    return mEnableHDR;
}

void AVOutput::setHDR(bool flag){
    mEnableHDR = flag;
    emit hdrModeChanged();
}

bool AVOutput::VR(){
    return mEnableVR;
}

void AVOutput::setVR(bool flag){
    mEnableVR = flag;
    emit vrModeChanged();
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
//    return QRect(0,0,w,h);
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
    return QRect(0,0,width(),height());
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
