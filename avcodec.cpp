#include "AVCodec.h"
#include <QDebug>


int readPacket(void *opaque, uint8_t *buf, int size){
    //qDebug() << "readSize : " << size;
    return 0;
}

int writePacket(void *opaque, uint8_t *buf, int size){
    //qDebug() << "writeSize : " << size;
    return 0;
}

int64_t seekPacket(void *opaque, int64_t offset, int whence){
    //qDebug() << "seek : ";
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
    , mMediaBufferMode(AVDefine::MediaBufferMode_Time)
    , mBufferSize(5000)
    , m_isDestroy(false)
    , mAudioSwrCtx(NULL)
    , mIsVideoBuffered(false)
    , mIsAudioBuffered(false)
    , mIsSubtitleBuffered(false)
    , mStatus(AVDefine::MediaStatus_UnknownStatus)
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
    , mCallback(NULL)

    , mAudioFrame(NULL)
    , mFrameYUV(NULL)
    , mYUVBuffer(NULL)
    , mIsInit(false)
{
//    avcodec_register_all();
//    avfilter_register_all();
//    av_register_all();
//    avformat_network_init();
//    av_log_set_callback(NULL);//不打印日志


    audioq.isInit = false;
    videoq.isInit = false;
    subtitleq.isInit = false;

    audioq.size = 0;
    audioq.nb_packets = 0;
    videoq.size = 0;
    videoq.nb_packets = 0;
    subtitleq.size = 0;
    subtitleq.nb_packets = 0;
}

AVCodec2::~AVCodec2(){
    m_isDestroy = true;
    mProcessThread.stop();
    release();
}


void AVCodec2::init(){
    if(avformat_open_input(&mFormatCtx, mFilename.toLatin1().constData(), NULL, NULL) != 0)
    {
//        qDebug() << "avformat_open_input 1";
        statusChanged(AVDefine::MediaStatus_NoMedia);
        return;
    }

    if(avformat_find_stream_info(mFormatCtx, NULL) < 0)
    {
        //qDebug() << "FindFail";
//        qDebug() << "avformat_open_input 2";
        statusChanged(AVDefine::MediaStatus_InvalidMedia);
        return;
    }

    mHasSubtitle = false;

    /* 寻找视频流 */
    int ret = av_find_best_stream(mFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &mVideoCodec, 0);
    if (ret < 0) {
        //qDebug() << "Cannot find a video stream in the input file";
    }else{
        mVideoIndex = ret;
        mVideoCodecCtx = avcodec_alloc_context3(mVideoCodec);
        if (!mVideoCodecCtx){
            //qDebug() << "create video context fail!";
//            qDebug() << "avformat_open_input 3";
            statusChanged(AVDefine::MediaStatus_InvalidMedia);
        }else{
            avcodec_parameters_to_context(mVideoCodecCtx, mFormatCtx->streams[mVideoIndex]->codecpar);

            mIsOpenVideoCodec = true;
            if(avcodec_open2(mVideoCodecCtx, mVideoCodec, NULL) < 0)
            {
                //qDebug() << "can't open video codec";
//                qDebug() << "avformat_open_input 4";
                statusChanged(AVDefine::MediaStatus_InvalidMedia);
                mIsOpenVideoCodec = false;
            }

            if(mIsOpenVideoCodec){
                if(mCallback){
                    mCallback->mediaHasVideoChanged();
                }
            }
        }
    }

    if(mCallback){
        mCallback->mediaDurationChanged(mFormatCtx->duration / 1000);
    }
//    qDebug() << "DURATION time : " <<mFormatCtx->duration / 1000;

    /* 寻找音频流 */
    ret = av_find_best_stream(mFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &mAudioCodec, 0);
    if (ret < 0) {
        //qDebug() << "Cannot find a audio stream in the input file";
        statusChanged(AVDefine::MediaStatus_InvalidMedia);
    }else{
        mAudioIndex = ret;

        /* create decoding context */
        mAudioCodecCtx = avcodec_alloc_context3(mAudioCodec);
        if (!mAudioCodecCtx){
            statusChanged(AVDefine::MediaStatus_InvalidMedia);
            //qDebug() << "create audio context fail!";
        }else{
            avcodec_parameters_to_context(mAudioCodecCtx, mFormatCtx->streams[mAudioIndex]->codecpar);
            mIsOpenAudioCodec = true;
            if(avcodec_open2(mAudioCodecCtx, mAudioCodec, NULL) < 0)
            {
                //qDebug() << "can't open audio codec";
                statusChanged(AVDefine::MediaStatus_InvalidMedia);
                mIsOpenAudioCodec = false;
            }

            if(mIsOpenAudioCodec){
                if(mCallback){
                    mCallback->mediaHasAudioChanged();
                }
            }
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
        format.setByteOrder(QAudioFormat::LittleEndian);

        mSourceAudioFormat = format;

        if(mCallback){
            mCallback->mediaUpdateAudioFormat(format);
        }

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
            statusChanged(AVDefine::MediaStatus_InvalidMedia);
        }else{
            swr_init(mAudioSwrCtx);
        }
        mAudioSwrCtxMutex.unlock();
        packet_queue_init(&audioq);
    }

    if(!mIsOpenVideoCodec || !mIsOpenAudioCodec){
        statusChanged(AVDefine::MediaStatus_InvalidMedia);
        return;
    }
    mIsInit = true;
    statusChanged(AVDefine::MediaStatus_Buffering);

    decodec();
}

void AVCodec2::release(){
    if(mFormatCtx != NULL){
        avformat_close_input(&mFormatCtx);
        avformat_free_context(mFormatCtx);
        mFormatCtx = NULL;
    }

    if(mAudioCodecCtx != NULL){
        avcodec_close(mAudioCodecCtx);
        avcodec_free_context(&mAudioCodecCtx);
        mAudioCodecCtx = NULL;
    }

    if(mVideoCodecCtx != NULL){
        avcodec_close(mVideoCodecCtx);
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

    if(mAudioFrame != NULL){
        av_frame_free(&mAudioFrame);
        mAudioFrame = NULL;
    }

    if(mFrameYUV != NULL){
        delete mFrameYUV;
        mFrameYUV = NULL;
    }

    mIsOpenAudioCodec = false;
    mIsOpenVideoCodec = false;
    mRotate = 0;
    mIsReadFinish = false;
    m_isDestroy = false;

    mAudioSwrCtxMutex.lock();
    if(mAudioSwrCtx != NULL){
        swr_close(mAudioSwrCtx);
        swr_free(&mAudioSwrCtx);
        mAudioSwrCtx = NULL;
    }
    mAudioSwrCtxMutex.unlock();

    mIsVideoBuffered = false;
    mIsAudioBuffered = false;
    mIsSubtitleBuffered = false;
    mStatus = AVDefine::MediaStatus_UnknownStatus;

    if(mAudioDstData != NULL)
    {
        delete mAudioDstData;
        mAudioDstData = NULL;
    }

    if(mVideoSwsCtx != NULL){
        sws_freeContext(mVideoSwsCtx);
        mVideoSwsCtx = NULL;
    }

    if(mYUVTransferBuffer != NULL){
        delete mYUVTransferBuffer;
        mYUVTransferBuffer = NULL;
    }

    if(mYUVBuffer != NULL){
        delete mYUVBuffer;
        mYUVBuffer = NULL;
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

    packet_queue_destroy(&audioq);
    packet_queue_destroy(&videoq);
    packet_queue_destroy(&subtitleq);
    audioq.size = 0;
    audioq.nb_packets = 0;
    videoq.size = 0;
    videoq.nb_packets = 0;
    subtitleq.size = 0;
    subtitleq.nb_packets = 0;
}

void AVCodec2::decodec(){
    if(!mIsOpenVideoCodec || !mIsOpenAudioCodec || !mIsInit){//必须同时存在音频和视频才能播放
        statusChanged(AVDefine::MediaStatus_InvalidMedia);
        return;
    }

    if(m_isDestroy || mIsReadFinish)
        return;

    if(mIsSeek){
        statusChanged(AVDefine::MediaStatus_Seeking);
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
        if(!mIsSeekd){
            mIsSeekd = true;
            if(mCallback){
                mCallback->mediaCanRenderFirstFrame();
            }
            statusChanged(AVDefine::MediaStatus_Buffered);
            statusChanged(AVDefine::MediaStatus_Seeked);
            statusChanged(AVDefine::MediaStatus_EndOfMedia);
        }else{
            if(mStatus == AVDefine::MediaStatus_Buffering){
                statusChanged(AVDefine::MediaStatus_Buffered);
            }
            statusChanged(AVDefine::MediaStatus_EndOfMedia);
        }
        return;
    }
    if(pkt.pts < 0){
        pkt.pts = pkt.dts;
    }

    if (pkt.stream_index == mVideoIndex){
        if(!mIsSeekd){
            int currentTime = av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * pkt.pts * 1000;
            if(currentTime < mSeekTime){
                mVideoCodecCtxMutex.lock();
                ret = avcodec_send_packet(mVideoCodecCtx, &pkt);
                if(ret != 0){
                }else{
                    AVFrame *tempFrame = av_frame_alloc();
                    while(avcodec_receive_frame(mVideoCodecCtx, tempFrame) == 0){
                        av_frame_unref(tempFrame);
                    }
                    av_frame_free(&tempFrame);
                }
                av_packet_unref(&pkt);
                mVideoCodecCtxMutex.unlock();
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
            if(mCallback){
                mCallback->mediaCanRenderFirstFrame();
            }
        }
    }else if(pkt.stream_index == mAudioIndex){
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
        if(mMediaBufferMode == AVDefine::MediaBufferMode_Time){
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
                if(!mIsSeekd){
                    mIsSeekd = true;
                    if(mCallback){
                        mCallback->mediaCanRenderFirstFrame();
                    }
                    statusChanged(AVDefine::MediaStatus_Buffered);
                    statusChanged(AVDefine::MediaStatus_Seeked);
                }
                if(mStatus == AVDefine::MediaStatus_Buffering){
                    statusChanged(AVDefine::MediaStatus_Buffered);
                }
            }
        }else if(mMediaBufferMode == AVDefine::MediaBufferMode_Packet){
            if(videoq.nb_packets < mBufferSize || audioq.nb_packets < mBufferSize)
                decodec();
            else{
                if(!mIsSeekd){
                    mIsSeekd = true;
                    if(mCallback){
                        mCallback->mediaCanRenderFirstFrame();
                    }
                    statusChanged(AVDefine::MediaStatus_Buffered);
                    statusChanged(AVDefine::MediaStatus_Seeked);
                }
                if(mStatus == AVDefine::MediaStatus_Buffering){
                    statusChanged(AVDefine::MediaStatus_Buffered);
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
        //emit updateInternetSpeed(mFormatCtx->pb->pos - mLastPos > 0 ? mFormatCtx->pb->pos - mLastPos : 0);
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

void AVCodec2::setMediaCallback(AVMediaCallback *callback){
    this->mCallback = callback;
}

void AVCodec2::load(){
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Init));
}

/** 设置播放速率，最大为8，最小为1.0 / 8 */
void AVCodec2::setPlayRate(float rate){
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_SetPlayRate,rate));
}

float AVCodec2::getPlayRate(){
    return this->mPlayRate;
}

void AVCodec2::seek(int ms){
    mProcessThread.clearAllTask();
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Seek,ms));
}

void AVCodec2::setBufferSize(int size){
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_SetBufferSize,size));
}

void AVCodec2::setMediaBufferMode(AVDefine::MediaBufferMode mode){
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_SetMediaBufferMode,mode));
}

void AVCodec2::checkBuffer(){
    if(!mIsReadFinish){
        //判断是否将设定的缓存装满，如果未装满的话，一直循环
        if(mIsOpenVideoCodec){
            int vstartTime = videoq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * videoq.first_pkt->pkt.pts * 1000;
            int vendTime = videoq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * videoq.last_pkt->pkt.pts * 1000;
            if(vendTime - vstartTime < mBufferSize){
                mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Decodec));
            }
        }

        if(mIsOpenAudioCodec){
            int astartTime = audioq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * audioq.first_pkt->pkt.pts * 1000;
            int aendTime = audioq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * audioq.last_pkt->pkt.pts * 1000;
            if(aendTime - astartTime < mBufferSize ){
                mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Decodec));
            }
        }


    }
}

/** 渲染第一帧 */
void AVCodec2::renderFirstFrame(){
    slotRenderFirstFrame();
    checkBuffer();
}

/** 渲染下一帧 */
void AVCodec2::renderNextFrame(){
    slotRenderNextFrame();
    checkBuffer();
}

/** 请求向音频buffer添加数据  */
void AVCodec2::requestAudioNextFrame(int len){
    slotRequestAudioNextFrame(len);
}

bool AVCodec2::hasVideo(){
    return mIsOpenVideoCodec;
}

bool AVCodec2::hasAudio(){
    return mIsOpenAudioCodec;
}

void AVCodec2::slotSeek(int ms){
    if(ms == 0){
        videoq.mutex->lock();
        int vstartTime = videoq.nb_packets == 0 ? -1 : av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * videoq.first_pkt->pkt.pts * 1000;
        videoq.mutex->unlock();
        audioq.mutex->lock();
        int astartTime = audioq.nb_packets == 0 ? -1 : av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * audioq.first_pkt->pkt.pts * 1000;
        audioq.mutex->unlock();
        if(vstartTime == astartTime && (vstartTime == ms)){
            statusChanged(AVDefine::MediaStatus_Seeked);
            return;
        }
    }

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

void AVCodec2::slotSetMediaBufferMode(AVDefine::MediaBufferMode mode){
    mMediaBufferMode = mode;
}

void AVCodec2::slotRenderFirstFrame(){
    slotRenderNextFrame();
}

void AVCodec2::slotSetPlayRate(float rate){
    if(rate > 8 || rate < 1.0 / 8 || mPlayRate == rate){
        return;
    }
    this->mPlayRate = rate;
    QAudioFormat temp = mSourceAudioFormat;
    temp.setSampleRate(temp.sampleRate() * mPlayRate);

    mAudioBufferMutex.lock();
    //清除音频buffer
    mAudioBuffer.clear();
    mAudioBufferMutex.unlock();

    if(mCallback){
        mCallback->mediaUpdateAudioFormat(temp);
    }
}



void AVCodec2::slotRenderNextFrame(){
    if(!mIsInit)
        return;
    if(!mIsOpenVideoCodec || !mIsOpenAudioCodec){
        statusChanged(AVDefine::MediaStatus_InvalidMedia);
        return;
    }
    if(m_isDestroy || mIsVideoPlayed)
        return;

    mVideoDecodecMutex.lock();
    AVPacket pkt;
    int isPacket = packet_queue_get(&videoq,&pkt,0);
    if(isPacket){
        mVideoCodecCtxMutex.lock();
        int ret = avcodec_send_packet(mVideoCodecCtx, &pkt);
        if(ret != 0){
            av_packet_unref(&pkt);
            mVideoCodecCtxMutex.unlock();
            mVideoDecodecMutex.unlock();
            return;
        }
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

            int width = mFrame->width;
            int height = mFrame->height;
            memset(mYUVTransferBuffer,0,width*height*3/2);
            int i,a;
            for (i = 0,a = 0; i<height; i++){
                memcpy(mYUVTransferBuffer + a,(const char *)(frame->data[0] + i * frame->linesize[0]),width);
                a += width;
            }
            for (i = 0; i<height / 2; i++){
                memcpy(mYUVTransferBuffer + a,(const char *)(frame->data[1] + i * frame->linesize[1]),width / 2);
                a += width / 2;
            }
            for (i = 0; i<height / 2; i++){
                memcpy(mYUVTransferBuffer + a,(const char *)(frame->data[2] + i * frame->linesize[2]),width / 2);
                a += width / 2;
            }
            av_frame_unref(mFrame);
            m_mutex.unlock();
            if(mCallback){
                mCallback->mediaUpdateVideoFrame(mYUVTransferBuffer,(void *)&vFormat);
            }
        }
        mVideoCodecCtxMutex.unlock();
    }else{
        if(!mIsReadFinish){
            statusChanged(AVDefine::MediaStatus_Buffering);
        }
        else{
            mIsVideoPlayed = true;
        }

        if(mIsVideoPlayed && mIsAudioPlayed && mIsSubtitlePlayed){
            emit statusChanged(AVDefine::MediaStatus_Played);
        }
    }
    if(isPacket)
        av_packet_unref(&pkt);
    mVideoDecodecMutex.unlock();
}

void AVCodec2::slotRequestAudioNextFrame(int len){
    if(!mIsInit)
        return;
    if(!mIsOpenVideoCodec || !mIsOpenAudioCodec){
        statusChanged(AVDefine::MediaStatus_InvalidMedia);
        return;
    }

    if(m_isDestroy || mIsAudioPlayed)
        return;

    mAudioBufferMutex.lock();
    int audioBufferLen = mAudioBuffer.length();
    mAudioBufferMutex.unlock();

    if(audioBufferLen < len){
        AVPacket pkt;
        int isPacket = packet_queue_get(&audioq,&pkt,0);
        if(isPacket){
            int ret = avcodec_send_packet(mAudioCodecCtx, &pkt);
            while (ret >= 0) {
                ret = avcodec_receive_frame(mAudioCodecCtx, mAudioFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;

                else if (ret < 0){
//                    qDebug() << "Error during decoding";
                    break;
                }

                int dataSize = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                if (dataSize < 0) {
//                    qDebug() << "Failed to calculate data size";
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
                mAudioBufferMutex.lock();
                mAudioBuffer.append((const char *)mAudioDstData,outsize);
                mAudioBufferMutex.unlock();
                av_frame_unref(mAudioFrame);
            }
            slotRequestAudioNextFrame(len);
        }else{
            if(!mIsReadFinish)
                statusChanged(AVDefine::MediaStatus_Buffering);
            else{
                mIsAudioPlayed = true;
            }

            if(mIsVideoPlayed && mIsAudioPlayed && mIsSubtitlePlayed){
                emit statusChanged(AVDefine::MediaStatus_Played);
            }
        }
        if(isPacket)
            av_packet_unref(&pkt);
    }else{
        mAudioBufferMutex.lock();
        QByteArray r = mAudioBuffer.left(len);
        mAudioBuffer.remove(0,len);
        mAudioBufferMutex.unlock();
        if(mCallback){
            mCallback->mediaUpdateAudioFrame(r);
        }
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

void AVCodec2::packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = new QMutex;
    q->cond = new QWaitCondition;
    q->size = 0;
    q->nb_packets = 0;
    q->first_pkt = NULL;
    q->last_pkt = NULL;
    q->isInit = true;
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
            q->size -= pkt1->pkt.size;
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
    if(q->nb_packets == 0 || !q->isInit)
        return;
    q->mutex->lock();
    for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1)
    {
        pkt1 = pkt->next;
        //av_free_packet(&pkt->pkt);
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
    if(q->isInit){
        packet_queue_flush(q);
        delete q->mutex;
        delete q->cond;
        q->isInit = false;
    }
}

void AVCodec2::statusChanged(AVDefine::MediaStatus status){
    mStatus = status;
    if(mCallback)
        mCallback->mediaStatusChanged(status);
}

void AVCodecTask::run(){
    switch(command){
    case AVCodecTaskCommand_Init:
        mCodec->init();
        break;
    case AVCodecTaskCommand_SetPlayRate:
        mCodec->slotSetPlayRate(param);
        break;
    case AVCodecTaskCommand_Seek:
        mCodec->slotSeek(param);
        break;
    case AVCodecTaskCommand_SetBufferSize:
        mCodec->slotSetBufferSize(param);
        break;
    case AVCodecTaskCommand_SetMediaBufferMode:
        mCodec->slotSetMediaBufferMode((AVDefine::MediaBufferMode)(int)param);
        break;
    case AVCodecTaskCommand_Decodec:
        mCodec->decodec();
        break;
    }
}
