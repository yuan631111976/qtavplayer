#include "avcodec.h"
#include <QImage>
#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QVideoFrame>
#include <QBuffer>



int readPacket(void *opaque, uint8_t *buf, int size){
    qDebug() << "readSize : " << size;
    return 0;
}

int writePacket(void *opaque, uint8_t *buf, int size){
    qDebug() << "writeSize : " << size;
    return 0;
}

int64_t seekPacket(void *opaque, int64_t offset, int whence){
    qDebug() << "seek : ";
    return 0;
}

AVCodec2::AVCodec2()
    : mAudioIndex(-1)
    , mVideoIndex(-1)
    , mFormatCtx(NULL)
    , mAudioCodecCtx(NULL)
    , mVideoCodecCtx(NULL)
    , mAudioCodec(NULL)
    , mVideoCodec(NULL)
    , mFrame(NULL)
    , mIsOpenAudioCodec(false)
    , mIsOpenVideoCodec(false)
    , mRotate(0)
    , mIsReadFinish(false)
    , mBufferMode(BufferMode::Time)
    , mBufferSize(5000)
    , m_isDestroy(false)
    , mAudioSwrCtx(NULL)
    , mIsVideoBuffered(false)
    , mIsAudioBuffered(false)
    , mIsSubtitleBuffered(false)
    , mStatus(UnknownStatus)
    , mAudioDstData(NULL)
    , mVideoSwsCtx(NULL)
    , mYUVTransferBuffer(NULL)
    , mIsSeek(false)
    , mSeekTime(-1)
    , mIsSeekd(true)
    , mIsAudioSeeked(true)
    , mIsVideoSeeked(true)
    , mIsSubtitleSeeked(true)
    , mIsAudioPlayed(true)
    , mIsVideoPlayed(true)
    , mIsSubtitlePlayed(true)
    , mAvioCtx(NULL)
    , mAvioBuffer(NULL)
    , mLastTime(0)
    , mLastPos(0)
    , mPlayRate(1.0)
{
    avcodec_register_all();
    avfilter_register_all();
    av_register_all();
    avformat_network_init();
    av_force_cpu_flags(av_get_cpu_flags());

    qRegisterMetaType<AVCodec2::BufferMode>("AVCodec2::BufferMode");
    qRegisterMetaType<BufferMode>("BufferMode");
    qRegisterMetaType<AVCodec2::Status>("AVCodec2::Status");
    qRegisterMetaType<Status>("Status");

    connect(this,SIGNAL(sigLoad()),this,SLOT(slotLoad()));
    connect(this,SIGNAL(sigSeek(int)),this,SLOT(slotSeek(int)));    
    connect(this,SIGNAL(sigSetBufferMode(BufferMode)),this,SLOT(slotSetBufferMode(BufferMode)));
    connect(this,SIGNAL(sigSetBufferSize(int)),this,SLOT(slotSetBufferSize(int)));
    connect(this,SIGNAL(sigCheckBuffer()),this,SLOT(slotCheckBuffer()));
    connect(this,SIGNAL(sigSetPlayRate(float)),this,SLOT(slotSetPlayRate(float)));


    connect(this,SIGNAL(sigRenderFirstFrame()),
            this,SLOT(slotRenderFirstFrame()),
            Qt::DirectConnection);
//    connect(this,SIGNAL(sigRenderFirstFrame()),
//            this,SLOT(slotRenderFirstFrame()));
    connect(this,SIGNAL(sigRenderNextFrame()),
            this,SLOT(slotRenderNextFrame()),
            Qt::DirectConnection);
    connect(this,SIGNAL(sigRequestAudioNextFrame(int)),
            this,SLOT(slotRequestAudioNextFrame(int)),
            Qt::DirectConnection);

    memset(&audioq,0,sizeof(PacketQueue));
    memset(&videoq,0,sizeof(PacketQueue));
    memset(&subtitleq,0,sizeof(PacketQueue));

//    yuv = YuvManager::GetInstance()->CreateYuv();
//    yuv2 = YuvManager::GetInstance()->CreateYuv();

    for(int i = 0;i < 1;i++){
        yuvs.push_back(YuvManager::GetInstance()->CreateYuv());
    }

    this->moveToThread(&mThread);
    mThread.start();
}

AVCodec2::~AVCodec2(){
    qDebug() << "~AVCodec2";
    m_isDestroy = true;
    if(mAudioDstData)
        delete mAudioDstData;
    mThread.quit();
    mThread.wait();
}

static int decode_interrupt_cb(void *ctx)
{
    qDebug() << "decode_interrupt_cb : " << ctx;
//    VideoState *is = ctx;
//    return is->abort_request;
}

void AVCodec2::init(){
//    mFormatCtx = avformat_alloc_context();
//    mFormatCtx->pb->
//    mFormatCtx->programs-
//    mFormatCtx->interrupt_callback.callback = decode_interrupt_cb;

//    av_probe_input_buffer()
//    avio_write();
//    avio_alloc_context
//    avio_alloc_context()

    if(avformat_open_input(&mFormatCtx, mFilename.toLatin1().constData(), NULL, NULL) != 0)
    {
        qDebug() << "OpenFail";
        sigStatusChanged(AVCodec2::NoMedia);
        return;
    }

    //设置av_read  av_write av_seek,以便知道下载速率,和下载进度

    av_log_set_callback(NULL);//不打印日志
    mAvioBuffer = new uint8_t[FFMPEG_AVIO_INBUFFER_SIZE];
    if (!mAvioBuffer) {
        sigStatusChanged(AVCodec2::NoMedia);
        return;
    }

    mAvioCtx = avio_alloc_context(mAvioBuffer, FFMPEG_AVIO_INBUFFER_SIZE,0,
                                  this, &readPacket, &writePacket, NULL);//将buffer读入 设置buffer回调函数
    if (!mAvioCtx) {
        qDebug() << "avio init fail!!!";
        sigStatusChanged(AVCodec2::NoMedia);
        return;
    }

//    AVIO_FLAG_DIRECT
//    mFormatCtx->avio_flags
    mFormatCtx->pb->write_packet = writePacket;
//    mFormatCtx->pb->read_packet = readPacket;
//    mFormatCtx->pb->seek = seekPacket;

//    mFormatCtx->pb = mAvioCtx;
//    mFormatCtx->flags |= AVFMT_FLAG_GENPTS;
//    mFormatCtx->pb = mAvioCtx;
//    mFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

    if(avformat_find_stream_info(mFormatCtx, NULL) < 0)
    {
        qDebug() << "FindFail";
        av_free(mAvioBuffer);
        sigStatusChanged(AVCodec2::NoMedia);
        return;
    }

//    av_probe_input_buffer();


//    /* 寻找视频流 */
//    int ret = av_find_best_stream(mFormatCtx, AVMEDIA_TYPE_SUBTITLE, -1, -1, &mVideoCodec, 0);
//    if (ret < 0) {
//        qDebug() << "Cannot find a subtitle stream in the input file";
//    }
    mHasSubtitle = false;

//    AVCodec
//    AVCodecID
//    AV_CODEC_ID_H264,
//    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);

    /* 寻找视频流 */
    int ret = av_find_best_stream(mFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &mVideoCodec, 0);
    if (ret < 0) {
        qDebug() << "Cannot find a video stream in the input file";
    }else{
        mVideoIndex = ret;
        /* create decoding context */
        mVideoCodecCtx = avcodec_alloc_context3(mVideoCodec);
        if (!mVideoCodecCtx)
            qDebug() << "create video context fail!";
        else{
            mVideoCodecCtx->thread_count = 3;
            avcodec_parameters_to_context(mVideoCodecCtx, mFormatCtx->streams[mVideoIndex]->codecpar);

            mIsOpenVideoCodec = true;
            if(avcodec_open2(mVideoCodecCtx, mVideoCodec, NULL) < 0)
            {
                qDebug() << "can't open video codec";
                mIsOpenVideoCodec = false;
            }

            if(mIsOpenVideoCodec)
                emit hasVideoChanged();
        }
    }


    emit durationChanged(mFormatCtx->duration / 1000);
//    qDebug() << "DURATION time : " <<mFormatCtx->duration / 1000;

    /* 寻找音频流 */
    ret = av_find_best_stream(mFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &mAudioCodec, 0);
    if (ret < 0) {
        qDebug() << "Cannot find a audio stream in the input file";
    }else{
        mAudioIndex = ret;

        /* create decoding context */
        mAudioCodecCtx = avcodec_alloc_context3(mAudioCodec);
        if (!mAudioCodecCtx)
            qDebug() << "create audio context fail!";
        else{
            avcodec_parameters_to_context(mAudioCodecCtx, mFormatCtx->streams[mAudioIndex]->codecpar);
            mIsOpenAudioCodec = true;
            if(avcodec_open2(mAudioCodecCtx, mAudioCodec, NULL) < 0)
            {
                qDebug() << "can't open audio codec";
                mIsOpenAudioCodec = false;
            }

            if(mIsOpenAudioCodec)
                emit hasAudioChanged();
        }
    }


    if(mIsOpenVideoCodec){
        mFrame = av_frame_alloc();
        AVDictionaryEntry *tag = NULL;
        tag = av_dict_get(mFormatCtx->streams[mVideoIndex]->metadata, "rotate", tag, 0);
        if(tag != NULL)
            mRotate = QString(tag->value).toInt();
        av_free(tag);

        vFormat.width = mVideoCodecCtx->width;
        vFormat.height = mVideoCodecCtx->height;
        vFormat.rotate = mRotate;
        vFormat.format = AV_PIX_FMT_YUV420P;
        vFormat.mutex = &m_mutex;

        int numBytes = av_image_get_buffer_size( AV_PIX_FMT_YUV420P, mVideoCodecCtx->width,mVideoCodecCtx->height, 1  );
        mYUVTransferBuffer = new char[numBytes];


        switch (mVideoCodecCtx->pix_fmt) {
        case AV_PIX_FMT_YUYV422 :
        case AV_PIX_FMT_YUV422P :
        case AV_PIX_FMT_YUV444P :
        case AV_PIX_FMT_YUV410P :
        case AV_PIX_FMT_YUV411P :
        case AV_PIX_FMT_YUVJ420P :
        case AV_PIX_FMT_YUVJ422P :
        case AV_PIX_FMT_YUVJ444P :
        case AV_PIX_FMT_YUV440P :
        case AV_PIX_FMT_YUV420P16LE :
        case AV_PIX_FMT_YUV420P16BE :
        case AV_PIX_FMT_YUV422P16LE :
        case AV_PIX_FMT_YUV422P16BE :
        case AV_PIX_FMT_YUV444P16LE :
        case AV_PIX_FMT_YUV444P16BE :
        case AV_PIX_FMT_YUV420P9BE :
        case AV_PIX_FMT_YUV420P9LE :
        case AV_PIX_FMT_YUV420P10BE :
        case AV_PIX_FMT_YUV420P10LE :
        case AV_PIX_FMT_YUV422P10BE :
        case AV_PIX_FMT_YUV422P10LE :
        case AV_PIX_FMT_YUV444P9BE :
        case AV_PIX_FMT_YUV444P9LE :
        case AV_PIX_FMT_YUV444P10BE :
        case AV_PIX_FMT_YUV444P10LE :
        case AV_PIX_FMT_YUV422P9BE :
        case AV_PIX_FMT_YUV422P9LE :
        case AV_PIX_FMT_YUVA420P9BE :
        case AV_PIX_FMT_YUVA420P9LE :
        case AV_PIX_FMT_YUVA422P9BE :
        case AV_PIX_FMT_YUVA422P9LE :
        case AV_PIX_FMT_YUVA444P9BE :
        case AV_PIX_FMT_YUVA444P9LE :
        case AV_PIX_FMT_YUVA420P10BE :
        case AV_PIX_FMT_YUVA420P10LE :
        case AV_PIX_FMT_YUVA422P10BE :
        case AV_PIX_FMT_YUVA422P10LE :
        case AV_PIX_FMT_YUVA444P10BE :
        case AV_PIX_FMT_YUVA444P10LE :
        case AV_PIX_FMT_YUVA420P16BE :
        case AV_PIX_FMT_YUVA420P16LE :
        case AV_PIX_FMT_YUVA422P16BE :
        case AV_PIX_FMT_YUVA422P16LE :
        case AV_PIX_FMT_YUVA444P16BE :
        case AV_PIX_FMT_YUVA444P16LE :
        case AV_PIX_FMT_YUV420P12BE :
        case AV_PIX_FMT_YUV420P12LE :
        case AV_PIX_FMT_YUV420P14BE :
        case AV_PIX_FMT_YUV420P14LE :
        case AV_PIX_FMT_YUV422P12BE :
        case AV_PIX_FMT_YUV422P12LE :
        case AV_PIX_FMT_YUV422P14BE :
        case AV_PIX_FMT_YUV422P14LE :
        case AV_PIX_FMT_YUV444P12BE :
        case AV_PIX_FMT_YUV444P12LE :
        case AV_PIX_FMT_YUV444P14BE :
        case AV_PIX_FMT_YUV444P14LE :
        case AV_PIX_FMT_YUV440P10LE :
        case AV_PIX_FMT_YUV440P10BE :
        case AV_PIX_FMT_YUV440P12LE :
        case AV_PIX_FMT_YUV440P12BE :
        case AV_PIX_FMT_YUV420P :
            break;
//        case AV_PIX_FMT_BGRA :
//        case AV_PIX_FMT_RGB24 :
//            break;

//            avio_alloc_context
        default: //AV_PIX_FMT_YUV420P
            mVideoSwsCtx = sws_getContext(
                mVideoCodecCtx->width,
                mVideoCodecCtx->height,
                mVideoCodecCtx->pix_fmt,
                mVideoCodecCtx->width,
                mVideoCodecCtx->height,
                AV_PIX_FMT_YUV420P,
                SWS_BICUBIC,NULL,NULL,NULL);
            mFrameYUV = av_frame_alloc();

            mYUVBuffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
            int buffSize = av_image_fill_arrays( mFrameYUV->data, mFrameYUV->linesize, mYUVBuffer, AV_PIX_FMT_YUV420P,mVideoCodecCtx->width,mVideoCodecCtx->height, 1 );
            if( buffSize < 1 )
            {
                av_frame_free( &mFrameYUV );
                av_free( mYUVBuffer );
                sws_freeContext(mVideoSwsCtx);
                mVideoSwsCtx = NULL;
                mYUVBuffer = NULL;
                mFrameYUV = NULL;
            }
            break;
        }
        packet_queue_init(&videoq);
    }


    if(mIsOpenAudioCodec){
        mAudioFrame = av_frame_alloc();

        int sampleRate = mAudioCodecCtx->sample_rate;
        int channels = mAudioCodecCtx->channels;

        QAudioFormat format;
        format.setCodec("audio/pcm");
        format.setSampleRate(sampleRate * mPlayRate);
        format.setChannelCount(channels);
        format.setSampleType(QAudioFormat::SignedInt);
        format.setSampleSize(16);
        format.setByteOrder(QAudioFormat::Endian::LittleEndian);

        mSourceAudioFormat = format;
        emit updateAudioFormat(format);

        mAudioSwrCtxMutex.lock();
        mAudioSwrCtx = swr_alloc_set_opts(
                    NULL,
                    mAudioCodecCtx->channel_layout,
                    AV_SAMPLE_FMT_S16,
                    sampleRate,
                    mAudioCodecCtx->channel_layout,
                    mAudioCodecCtx->sample_fmt,
                    sampleRate,
                    0,NULL);
        if(!mAudioSwrCtx)
        {
            qDebug() << "Could not set options for resample context.";
        }else{
            swr_init(mAudioSwrCtx);
        }
        mAudioSwrCtxMutex.unlock();
        packet_queue_init(&audioq);
    }

    sigStatusChanged(AVCodec2::Buffering);
    decodec();
}

void AVCodec2::release(){
    if(mFormatCtx != NULL){
        avformat_free_context(mFormatCtx);
        mFormatCtx = NULL;
    }

    if(mAudioCodecCtx != NULL){
        avcodec_free_context(&mAudioCodecCtx);
        mAudioCodecCtx = NULL;
    }

    if(mVideoCodecCtx != NULL){
        avcodec_free_context(&mVideoCodecCtx);
        mVideoCodecCtx = NULL;
    }

    if(mAudioCodec != NULL){
        av_free(mAudioCodec);
        mAudioCodec = NULL;
    }

    if(mVideoCodec != NULL){
        av_free(mVideoCodec);
        mVideoCodec = NULL;
    }

    if(mFrame != NULL){
        av_frame_free(&mFrame);
        mFrame = NULL;
    }

    mIsOpenAudioCodec = false;
    mIsOpenVideoCodec = false;
    mRotate = 0;
    mIsReadFinish = false;
    m_isDestroy = false;

    mAudioSwrCtxMutex.lock();
    if(mAudioSwrCtx != NULL){
        av_free(mAudioSwrCtx);
        mAudioSwrCtx = NULL;
    }
    mAudioSwrCtxMutex.unlock();

    mIsVideoBuffered = false;
    mIsAudioBuffered = false;
    mIsSubtitleBuffered = false;
    mStatus = UnknownStatus;

    if(mAudioDstData != NULL)
    {
        delete mAudioDstData;
        mAudioDstData = NULL;
    }

    if(mVideoSwsCtx != NULL){
        av_free(mVideoSwsCtx);
        mVideoSwsCtx = NULL;
    }

    if(mYUVTransferBuffer != NULL){
        delete mYUVTransferBuffer;
        mYUVTransferBuffer = NULL;
    }

    mIsSeek = false;
    mSeekTime = -1;
    mIsSeekd = true;
    mIsAudioSeeked = true;
    mIsVideoSeeked = true;
    mIsSubtitleSeeked = true;
    mIsAudioPlayed = true;
    mIsVideoPlayed = true;
    mIsSubtitlePlayed = true;

    if(mAvioCtx != NULL){
        av_free(mAvioCtx);
        mAvioCtx = NULL;
    }

    if(mAvioBuffer != NULL){
        delete mAvioBuffer;
        mAvioBuffer = NULL;
    }

    mLastTime = 0;
    mLastPos = 0;
}

void AVCodec2::decodec(){
    if(m_isDestroy || mIsReadFinish)
        return;

    if(mIsSeek){
        sigStatusChanged(AVCodec2::Seeking);
        av_seek_frame(mFormatCtx,-1,mSeekTime * 1000,AVSEEK_FLAG_BACKWARD);
        mIsSeek = false;
        mIsVideoSeeked = false;
        mIsAudioSeeked = false;
        if(mHasSubtitle)
            mIsSubtitleSeeked = false;
        mIsSeekd = false;
    }

    AVPacket pkt;
    int ret = av_read_frame(mFormatCtx, &pkt);
    mIsReadFinish = ret < 0;
    if(mIsReadFinish){
        qDebug() << "所有数据加载完成!";
        if(mStatus == AVCodec2::Buffering){
            sigStatusChanged(AVCodec2::Buffered);
        }
        return;
    }

    if(pkt.pts < 0){
        pkt.pts = pkt.dts;
    }

    if (pkt.stream_index == mVideoIndex){
        //qDebug() << " video : " << av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * pkt.pts * 1000;
        if(!mIsSeekd){
            int currentTime = av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * pkt.pts * 1000;
            if(currentTime < mSeekTime){
                ret = avcodec_send_packet(mVideoCodecCtx, &pkt);
                if(ret != 0){
                }else{
                    while(avcodec_receive_frame(mVideoCodecCtx, mFrame) == 0){}
                }
                av_packet_unref(&pkt);
            }else{
                packet_queue_put(&videoq, &pkt);
                mIsVideoSeeked = true;
                mIsVideoPlayed = false;
            }
        }else{
            if(pkt.pts >= 0){
                packet_queue_put(&videoq, &pkt);
                mIsVideoPlayed = false;
            }
        }

        if(pkt.pts == 0)
        {
            emit canRenderFirstFrame();
        }
    }else if(pkt.stream_index == mAudioIndex){
        //qDebug() << " audio : " << av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * pkt.pts * 1000;
        if(!mIsSeekd){
            qint64 audioTime = av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * pkt.pts * 1000;
            if(audioTime < mSeekTime){ //如果音频时间小于拖动的时间，则丢掉音频包
                av_packet_unref(&pkt);
            }else{
                mIsAudioSeeked = true;
                mIsAudioPlayed = false;
                packet_queue_put(&audioq, &pkt);
            }
        }else{
            mIsAudioPlayed = false;
            packet_queue_put(&audioq, &pkt);
        }
    }
    else if(mFormatCtx->streams[pkt.stream_index]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE){
        packet_queue_put(&subtitleq, &pkt);
        av_packet_unref(&pkt);
        mIsSubtitleSeeked = true;
        mIsSubtitlePlayed = false;
    }
    else{
        av_packet_unref(&pkt);
    }

    if(!mIsVideoSeeked || !mIsAudioSeeked || !mIsSubtitleSeeked){ //如果其中某一个元素未完成拖动，则继续解码s
        decodec();
    }else{
        if(!mIsSeekd){
            mIsSeekd = true;
            emit canRenderFirstFrame();
            sigStatusChanged(AVCodec2::Seeked);
        }
        if(mBufferMode == BufferMode::Time){
            //判断是否将设定的缓存装满，如果未装满的话，一直循环
            videoq.mutex->lock();
            int vstartTime = videoq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * videoq.first_pkt->pkt.pts * 1000;
            int vendTime = videoq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * videoq.last_pkt->pkt.pts * 1000;
            videoq.mutex->unlock();

            audioq.mutex->lock();
            int astartTime = audioq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * audioq.first_pkt->pkt.pts * 1000;
            int aendTime = audioq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * audioq.last_pkt->pkt.pts * 1000;
            audioq.mutex->unlock();

            if(vendTime - vstartTime < mBufferSize ||
                    aendTime - astartTime < mBufferSize ){
                decodec();
            }else{
                if(mStatus == AVCodec2::Buffering){
                    sigStatusChanged(AVCodec2::Buffered);
                }
            }
        }else if(mBufferMode == BufferMode::Packet){
            if(videoq.nb_packets < mBufferSize || audioq.nb_packets < mBufferSize)
                decodec();
            else{
                if(mStatus == AVCodec2::Buffering){
                    sigStatusChanged(AVCodec2::Buffered);
                }
            }
        }
    }

    if(!mTime.isValid())
        mTime.start();

    int elapsed = mTime.elapsed();
    if(elapsed - mLastTime >= 1000){ //1秒钟
//        qDebug() << "bytes_read : " << mFormatCtx->pb->bytes_read
//                 << " pos : " << mFormatCtx->pb->pos
//                 << " max size : " << mFormatCtx->pb->maxsize
//                 << " written : " << mFormatCtx->pb->written
//                 << " download speed : " << (mFormatCtx->pb->pos - mLastPos) / 1024 << "kb/s";
        emit updateInternetSpeed(mFormatCtx->pb->pos - mLastPos > 0 ? mFormatCtx->pb->pos - mLastPos : 0);
        mLastTime = 0;
        mTime.restart();
        mLastPos = mFormatCtx->pb->pos;
    }
}

void AVCodec2::setFilename(const QString &source){
    if(mFilename.size() != 0){
        if(audioq.nb_packets > 0)
            packet_queue_flush(&audioq);
        if(videoq.nb_packets > 0)
            packet_queue_flush(&videoq);
        if(subtitleq.nb_packets > 0)
            packet_queue_flush(&subtitleq);
        memset(&audioq,0,sizeof(PacketQueue));
        memset(&videoq,0,sizeof(PacketQueue));
        memset(&subtitleq,0,sizeof(PacketQueue));
        release();//释放所有资源
    }
    mFilename = source;
}

void AVCodec2::load(){
    emit sigLoad();
}

void AVCodec2::play(){
    emit sigPlay();
}

/** 设置播放速率，最大为8，最小为1.0 / 8 */
void AVCodec2::setPlayRate(float rate){
    emit sigSetPlayRate(rate);
}

float AVCodec2::getPlayRate(){
    return this->mPlayRate;
}

void AVCodec2::seek(int ms){
    emit sigSeek(ms);
}


void AVCodec2::setBufferSize(int size){
    emit sigSetBufferSize(size);
}

void AVCodec2::setBufferMode(BufferMode mode){
    emit sigSetBufferMode(mode);
}

void AVCodec2::checkBuffer(){
    if(!mIsReadFinish){
        //判断是否将设定的缓存装满，如果未装满的话，一直循环
        int vstartTime = videoq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * videoq.first_pkt->pkt.pts * 1000;
        int vendTime = videoq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * videoq.last_pkt->pkt.pts * 1000;

        int astartTime = audioq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * audioq.first_pkt->pkt.pts * 1000;
        int aendTime = audioq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * audioq.last_pkt->pkt.pts * 1000;

        if(vendTime - vstartTime < mBufferSize ||
                aendTime - astartTime < mBufferSize ){
            emit sigCheckBuffer();
        }
    }
}

/** 渲染第一帧 */
void AVCodec2::renderFirstFrame(){
    emit sigRenderFirstFrame();
    checkBuffer();
}

/** 渲染下一帧 */
void AVCodec2::renderNextFrame(){
    emit sigRenderNextFrame();
    checkBuffer();
}

/** 请求向音频buffer添加数据  */
void AVCodec2::requestAudioNextFrame(int len){
    emit sigRequestAudioNextFrame(len);
}

bool AVCodec2::hasVideo(){
    return mIsOpenVideoCodec;
}

bool AVCodec2::hasAudio(){
    return mIsOpenAudioCodec;
}

void AVCodec2::slotLoad(){
    init();
}

void AVCodec2::slotSeek(int ms){
    //清除队列
    if(audioq.nb_packets > 0)
        packet_queue_flush(&audioq);
    if(videoq.nb_packets > 0)
        packet_queue_flush(&videoq);
    if(subtitleq.nb_packets > 0)
        packet_queue_flush(&subtitleq);
    mIsSeek = true;
    mSeekTime = ms;
    mIsReadFinish = false;
    checkBuffer();
}

void AVCodec2::slotSetBufferSize(int size){
    mBufferSize = size;
}

void AVCodec2::slotSetBufferMode(BufferMode mode){
    mBufferMode = mode;
}

void AVCodec2::slotCheckBuffer(){
    decodec();
}

void AVCodec2::slotRenderFirstFrame(){
    slotRenderNextFrame();
}

void AVCodec2::slotSetPlayRate(float rate){
    if(rate > 8 || rate < 1.0 / 8 || mPlayRate == rate){
        return;
    }
    this->mPlayRate = rate;

    mAudioSwrCtxMutex.lock();
    QAudioFormat temp = mSourceAudioFormat;
    temp.setSampleRate(temp.sampleRate() * mPlayRate);
    emit updateAudioFormat(temp);
    mAudioSwrCtxMutex.unlock();
    //清除音频buffer
    mAudioBuffer.clear();
}

void AVCodec2::slotRenderNextFrame(){
    if(m_isDestroy || mIsVideoPlayed)
        return;

    mVideoDecodecMutex.lock();

    AVPacket pkt;
    int isPacket = packet_queue_get(&videoq,&pkt,0);
    if(isPacket){
        int ret = avcodec_send_packet(mVideoCodecCtx, &pkt);
        if(ret != 0){
            av_packet_unref(&pkt);
            mVideoDecodecMutex.unlock();
            return;
        }
        av_frame_unref(mFrame);
        while(avcodec_receive_frame(mVideoCodecCtx, mFrame) == 0){
            m_mutex.lock();
            AVFrame *frame = mFrame;
            if(mVideoSwsCtx != NULL){
                ret = sws_scale(mVideoSwsCtx,
                          mFrame->data,
                          mFrame->linesize,
                          0,
                          mFrame->height,
                          mFrameYUV->data,
                          mFrameYUV->linesize);
                frame = mFrameYUV;
            }

            list<Yuv*>::iterator Begin_i = yuvs.begin();
            list<Yuv*>::iterator End_i = yuvs.end();
            while(Begin_i != End_i){
                Yuv *yuv = *Begin_i;
                yuv->Mutex.lock();
                yuv->YData_pc1 = frame->data[0];
                yuv->YSize_n4 = frame->linesize[0];

                yuv->UData_pc1 = frame->data[1];
                yuv->USize_n4 = frame->linesize[1];

                yuv->VData_pc1 = frame->data[2];
                yuv->VSize_n4 = frame->linesize[2];

                yuv->Width_n4 = mFrame->width;
                yuv->Height_n4 = mFrame->height;

                YuvManager::GetInstance()->UpdateYuv(yuv);
                yuv->Mutex.unlock();
                ++Begin_i;
            }
//            yuv->Mutex.lock();
//            yuv->YData_pc1 = frame->data[0];
//            yuv->YSize_n4 = frame->linesize[0];

//            yuv->UData_pc1 = frame->data[1];
//            yuv->USize_n4 = frame->linesize[1];

//            yuv->VData_pc1 = frame->data[2];
//            yuv->VSize_n4 = frame->linesize[2];

//            yuv->Width_n4 = mFrame->width;
//            yuv->Height_n4 = mFrame->height;

//            YuvManager::GetInstance()->UpdateYuv(yuv);
//            yuv->Mutex.unlock();


//            yuv2->Mutex.lock();
//            yuv2->YData_pc1 = frame->data[0];
//            yuv2->YSize_n4 = frame->linesize[0];

//            yuv2->UData_pc1 = frame->data[1];
//            yuv2->USize_n4 = frame->linesize[1];

//            yuv2->VData_pc1 = frame->data[2];
//            yuv2->VSize_n4 = frame->linesize[2];

//            yuv2->Width_n4 = mFrame->width;
//            yuv2->Height_n4 = mFrame->height;

//            YuvManager::GetInstance()->UpdateYuv(yuv2);
//            yuv2->Mutex.unlock();

            m_mutex.unlock();


//            int width = mFrame->width;
//            int height = mFrame->height;
//            memset(mYUVTransferBuffer,0,width*height*3/2);
//            int i,a;
//            for (i = 0,a = 0; i<height; i++){
//                memcpy(mYUVTransferBuffer + a,(const char *)(frame->data[0] + i * frame->linesize[0]),width);
//                a += width;
//            }
//            for (i = 0; i<height / 2; i++){
//                memcpy(mYUVTransferBuffer + a,(const char *)(frame->data[1] + i * frame->linesize[1]),width / 2);
//                a += width / 2;
//            }
//            for (i = 0; i<height / 2; i++){
//                memcpy(mYUVTransferBuffer + a,(const char *)(frame->data[2] + i * frame->linesize[2]),width / 2);
//                a += width / 2;
//            }
//            av_frame_unref(mFrame);
//            m_mutex.unlock();
//            emit updateVideoFrame(mYUVTransferBuffer,&vFormat);
        }
    }else{
        if(!mIsReadFinish)
            sigStatusChanged(AVCodec2::Buffering);
        else{
            mIsVideoPlayed = true;
        }

        if(mIsVideoPlayed && mIsAudioPlayed && mIsSubtitlePlayed){
            emit sigStatusChanged(AVCodec2::Played);
        }
    }
    if(isPacket)
        av_packet_unref(&pkt);
    mVideoDecodecMutex.unlock();
}

void AVCodec2::slotRequestAudioNextFrame(int len){
    if(m_isDestroy || mIsAudioPlayed)
        return;
    if(mAudioBuffer.length() < len){
        AVPacket pkt;
        int isPacket = packet_queue_get(&audioq,&pkt,0);
        if(isPacket){
            int ret = avcodec_send_packet(mAudioCodecCtx, &pkt);
            while (ret >= 0) {
                ret = avcodec_receive_frame(mAudioCodecCtx, mAudioFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;

                else if (ret < 0){
                    //qDebug() << "Error during decoding";
                    break;
                }

                int dataSize = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                if (dataSize < 0) {
                    //qDebug() << "Failed to calculate data size";
                    break;
                }

                int outsize = dataSize * mAudioFrame->nb_samples * mAudioCodecCtx->channels;
                if(mAudioDstData == NULL){
                    mAudioDstData = new uint8_t[outsize];
                }

                mAudioSwrCtxMutex.lock();
                int ret2 = swr_convert(
                            mAudioSwrCtx,
                            &mAudioDstData,
                            outsize,
                           (const uint8_t**)mAudioFrame->data,
                            mAudioFrame->nb_samples
                            );
                mAudioSwrCtxMutex.unlock();
                outsize = ret2 * mAudioCodecCtx->channels * dataSize;
                mAudioBuffer.append((const char *)mAudioDstData,outsize);
                av_frame_unref(mAudioFrame);
            }
            slotRequestAudioNextFrame(len);
        }else{
            if(!mIsReadFinish)
                sigStatusChanged(AVCodec2::Buffering);
            else{
                mIsAudioPlayed = true;
            }

            if(mIsVideoPlayed && mIsAudioPlayed && mIsSubtitlePlayed){
                emit sigStatusChanged(AVCodec2::Played);
            }
        }
        if(isPacket)
            av_packet_unref(&pkt);
    }else{
        QByteArray r = mAudioBuffer.left(len);
        mAudioBuffer.remove(0,len);
        emit updateAudioFrame(r);
    }
}

int AVCodec2::getCurrentVideoTime(){
    int time = -1;
    videoq.mutex->lock();
    if(videoq.nb_packets > 0){
        time = av_q2d(mFormatCtx->streams[mVideoIndex]->time_base) * videoq.first_pkt->pkt.pts * 1000;
    }
    videoq.mutex->unlock();
    return time;
}

int AVCodec2::getWidth(){
    return (mRotate == 0 || mRotate == 180 ) ? mVideoCodecCtx->width : mVideoCodecCtx->height;
}

int AVCodec2::getHeight(){
    return (mRotate == 0 || mRotate == 180 ) ? mVideoCodecCtx->height : mVideoCodecCtx->width;
}

void AVCodec2::packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = new QMutex;
    q->cond = new QWaitCondition;
    q->size = 0;
    q->nb_packets = 0;
    q->first_pkt = NULL;
    q->last_pkt = NULL;
}

int AVCodec2::packet_queue_put(PacketQueue *q, AVPacket *pkt) {
    AVPacketList *pkt1;
    if (av_dup_packet(pkt) < 0) {
        return -1;
    }
    pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    q->mutex->lock();

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;

    q->cond->wakeOne();

    q->mutex->unlock();
    return 0;
}

int AVCodec2::packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
    AVPacketList *pkt1;
    int ret;

    q->mutex->lock();
    for (;;) {
        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size;\
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            q->cond->wait(q->mutex);
        }
    }

    q->mutex->unlock();
    return ret;
}

void AVCodec2::packet_queue_flush(PacketQueue *q)
{
    AVPacketList *pkt, *pkt1;

    q->mutex->lock();
    for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1)
    {
        pkt1 = pkt->next;
//        av_free_packet(&pkt->pkt);
        av_packet_unref(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    q->mutex->unlock();
}

void AVCodec2::packet_queue_destroy(PacketQueue *q)
{
    packet_queue_flush(q);
    delete q->mutex;
    delete q->cond;
}

void AVCodec2::sigStatusChanged(AVCodec2::Status status){
    mStatus = status;
    emit statusChanged(status);
}
