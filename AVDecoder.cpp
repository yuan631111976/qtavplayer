#include "AVDecoder.h"
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

static int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
{
    AVDecoder *opaque = (AVDecoder *) ctx->opaque;
    int err = 0;

    if ((err = av_hwdevice_ctx_create(&opaque->mHWDeviceCtx, type,
                                      NULL, NULL, 0)) < 0) {
        qDebug() << "Failed to create specified HW device.\n";
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(opaque->mHWDeviceCtx);
    return err;
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts)
{
    AVDecoder *opaque = (AVDecoder *) ctx->opaque;
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == opaque->mHWPixFormat)
            return *p;
    }
    qDebug() << "Failed to get HW surface format.\n";
    return AV_PIX_FMT_NONE;
}
AVDecoder::AVDecoder()
    : mAudioIndex(-1)
    , mVideoIndex(-1)
    , mFormatCtx(NULL)
    , mAudioCodecCtx(NULL)
    , mVideoCodecCtx(NULL)
    , mAudioCodec(NULL)
    , mVideoCodec(NULL)
    , mFrame(NULL)
    , mFrame1(NULL)
    , mHWFrame(NULL)
    , mIsOpenAudioCodec(false)
    , mIsOpenVideoCodec(false)
    , mRotate(0)
    , mIsReadFinish(false)
    , mMediaBufferMode(AVDefine::MediaBufferMode_Time)
    , mBufferSize(5000)
    , mIsDestroy(false)
    , mAudioSwrCtx(NULL)
    , mIsVideoBuffered(false)
    , mIsAudioBuffered(false)
    , mIsSubtitleBuffered(false)
    , mStatus(AVDefine::MediaStatus_UnknownStatus)
    , mAudioDstData(NULL)
    , mVideoSwsCtx(NULL)
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
    , mIsSupportHw(false)
    , mHWDeviceCtx(NULL)
    , mIsEnableHwDecode(false)
    , mFrameIndex(0)
    , mVideoDecodedCount(0)
    , mIsVideoLoadedCompleted(true)
    , mIsAudioLoadedCompleted(true)
{
#ifdef LIBAVUTIL_VERSION_MAJOR
#if (LIBAVUTIL_VERSION_MAJOR < 56)
    avcodec_register_all();
    avfilter_register_all();
    av_register_all();
    avformat_network_init();
//    av_log_set_callback(NULL);//不打印日志
#endif
#endif


    packet_queue_destroy(&audioq);
    packet_queue_destroy(&videoq);
    packet_queue_destroy(&subtitleq);
}

AVDecoder::~AVDecoder(){
    mIsDestroy = true;
    mProcessThread.stop();
    int i = 0;
    while(!mProcessThread.isRunning() && i++ < 200){
        QThread::msleep(1);
    }
    release(true);
}


void AVDecoder::init(){
    if(mIsInit)
        return;
    if(avformat_open_input(&mFormatCtx, mFilename.toStdString().c_str(), NULL, NULL) != 0)
    {
        qDebug() << "media open error : " << mFilename.toStdString().data();
        statusChanged(AVDefine::MediaStatus_NoMedia);
        return;
    }

    if(avformat_find_stream_info(mFormatCtx, NULL) < 0)
    {
        qDebug() << "media find stream error : " << mFilename.toStdString().data();
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

            mVideoCodecCtx->thread_count = 0;
#ifdef ENABLE_HW
            if(mIsEnableHwDecode){
                for (int i = 0;; i++) {
                    const AVCodecHWConfig *config = avcodec_get_hw_config(mVideoCodec, i);
                    if (!config) {
                        qDebug() << "------------------------ hw : 不支持硬解";
                        break;
                    }
                    if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type != AV_HWDEVICE_TYPE_NONE) {
                        qDebug() << "------------------------ support hw";
                        mIsSupportHw = true;
                        mVideoCodecCtx->opaque = (void *) this;
                        mHWPixFormat = config->pix_fmt;
                        mVideoCodecCtx->get_format = get_hw_format;
                        hw_decoder_init(mVideoCodecCtx, config->device_type);
                        break;
                    }
                }
            }
#endif

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
//        statusChanged(AVDefine::MediaStatus_InvalidMedia);
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

//    mIsOpenAudioCodec = false;
//    mIsOpenVideoCodec = false;
    if(!mIsOpenVideoCodec && !mIsOpenAudioCodec){ //如果即没有音频，也没有视频，则通知状态无效
        statusChanged(AVDefine::MediaStatus_InvalidMedia);
        release();//释放所有资源
        return;
    }

    if(mIsOpenVideoCodec){
        mFrame = av_frame_alloc();
        mFrame1 = av_frame_alloc();
        AVDictionaryEntry *tag = NULL;
        tag = av_dict_get(mFormatCtx->streams[mVideoIndex]->metadata, "rotate", tag, 0);
        if(tag != NULL)
            mRotate = QString(tag->value).toInt();
        av_free(tag);

        vFormat.width = mVideoCodecCtx->width;
        vFormat.height = mVideoCodecCtx->height;
        vFormat.rotate = mRotate;
        vFormat.format = mVideoCodecCtx->pix_fmt;

        qDebug() <<"----------------------- : " << mVideoCodecCtx->pix_fmt << ":" << AV_PIX_FMT_YUV420P10LE;
        switch (mVideoCodecCtx->pix_fmt) {
        case AV_PIX_FMT_YUV420P :
        case AV_PIX_FMT_YUVJ420P :
        case AV_PIX_FMT_YUV422P :
        case AV_PIX_FMT_YUVJ422P :
        case AV_PIX_FMT_YUV444P :
        case AV_PIX_FMT_YUVJ444P :
        case AV_PIX_FMT_GRAY8 :
        case AV_PIX_FMT_UYVY422 :
        case AV_PIX_FMT_YUV420P10LE :
            break;
//        case AV_PIX_FMT_BGRA :
//        case AV_PIX_FMT_RGB24 :
//            break;

//            avio_alloc_context
        default: //AV_PIX_FMT_YUV420P
//            mVideoSwsCtx = sws_getContext(
//                mVideoCodecCtx->width,
//                mVideoCodecCtx->height,
//                mVideoCodecCtx->pix_fmt,
//                mVideoCodecCtx->width,
//                mVideoCodecCtx->height,
//                AV_PIX_FMT_YUV420P,
//                SWS_BICUBIC,NULL,NULL,NULL);
//            mFrameYUV = av_frame_alloc();


//            int numBytes = av_image_get_buffer_size( AV_PIX_FMT_YUV420P, mVideoCodecCtx->width,mVideoCodecCtx->height, 1  );
//            mYUVBuffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
//            int buffSize = av_image_fill_arrays( mFrameYUV->data, mFrameYUV->linesize, mYUVBuffer, AV_PIX_FMT_YUV420P,mVideoCodecCtx->width,mVideoCodecCtx->height, 1 );
//            if( buffSize < 1 )
//            {
//                av_frame_free( &mFrameYUV );
//                av_free( mYUVBuffer );
//                sws_freeContext(mVideoSwsCtx);
//                mVideoSwsCtx = NULL;
//                mYUVBuffer = NULL;
//                mFrameYUV = NULL;
//            }
            break;
        }
        packet_queue_init(&videoq);
        mIsVideoLoadedCompleted = false;
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
        mIsAudioLoadedCompleted = false;
    }else{
        QAudioFormat format;
        format.setCodec("audio/pcm");
        format.setSampleRate(8000 * mPlayRate);
        format.setChannelCount(1);
        format.setSampleType(QAudioFormat::SignedInt);
        format.setSampleSize(16);
        format.setByteOrder(QAudioFormat::LittleEndian);
        mSourceAudioFormat = format;
        if(mCallback){
            mCallback->mediaUpdateAudioFormat(format);
        }
    }

    mVideoDecodedCount = 0;
    mIsInit = true;
    statusChanged(AVDefine::MediaStatus_Buffering);
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Decodec));
}

void AVDecoder::release(bool isDeleted){
//    qDebug() << "-----------------1";
    if(mFormatCtx != NULL){
        avformat_close_input(&mFormatCtx);
        avformat_free_context(mFormatCtx);
        mFormatCtx = NULL;
    }
//    qDebug() << "-----------------2";
    if(mAudioCodecCtx != NULL){
        avcodec_close(mAudioCodecCtx);
        avcodec_free_context(&mAudioCodecCtx);
        mAudioCodecCtx = NULL;
    }
//    qDebug() << "-----------------3";
    if(mVideoCodecCtx != NULL){
        avcodec_close(mVideoCodecCtx);
        avcodec_free_context(&mVideoCodecCtx);
        mVideoCodecCtx = NULL;
    }
//    qDebug() << "-----------------4";
    if(mAudioCodec != NULL){
        av_free(mAudioCodec);
        mAudioCodec = NULL;
    }
//    qDebug() << "-----------------5";
    if(mVideoCodec != NULL){
        av_free(mVideoCodec);
        mVideoCodec = NULL;
    }
//    qDebug() << "-----------------6";
    if(mFrame != NULL){
        av_frame_free(&mFrame);
        mFrame = NULL;
    }
//    qDebug() << "-----------------7";
    if(mFrame1 != NULL){
        av_frame_free(&mFrame1);
        mFrame1 = NULL;
    }
//    qDebug() << "-----------------8";
    if(mAudioFrame != NULL){
        av_frame_free(&mAudioFrame);
        mAudioFrame = NULL;
    }
//    qDebug() << "-----------------9";
    if(mFrameYUV != NULL){
        delete mFrameYUV;
        mFrameYUV = NULL;
    }
//    qDebug() << "-----------------10";
    mIsOpenAudioCodec = false;
    mIsOpenVideoCodec = false;
    mRotate = 0;
    mIsReadFinish = false;

    if(!isDeleted){
        mIsDestroy = false;
    }

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
    mIsAudioLoadedCompleted = true;
    mIsVideoLoadedCompleted = true;

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
}

void AVDecoder::decodec(){
    if((!mIsOpenVideoCodec && !mIsOpenAudioCodec) || !mIsInit){//必须同时存在音频和视频才能播放
        statusChanged(AVDefine::MediaStatus_InvalidMedia);
        return;
    }

    if(mIsDestroy || mIsReadFinish)
        return;

    if(mIsSeek){
//        qDebug() <<"------------- seek";
        statusChanged(AVDefine::MediaStatus_Seeking);
        av_seek_frame(mFormatCtx,-1,mSeekTime * 1000,AVSEEK_FLAG_BACKWARD);
        mIsSeek = false;
//        mIsVideoSeeked = false;
//        mIsAudioSeeked = false;
        if(mHasSubtitle)
            mIsSubtitleSeeked = false;
        mIsSeekd = false;
    }

    mIsVideoSeeked = true;
    mIsAudioSeeked = true;

    AVPacket pkt;
    int ret = av_read_frame(mFormatCtx, &pkt);
    mIsReadFinish = ret < 0;
    if(mIsReadFinish){
        qDebug() << "--------------------- decoded completed";
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

//    qDebug() << "------------------------------- decodec:" << pkt.stream_index << ":" << mFormatCtx->streams[pkt.stream_index]->codec->codec_type;
    if (pkt.stream_index == mVideoIndex && mIsOpenVideoCodec){
//        qDebug() <<"------------- video";
        mIsVideoLoadedCompleted = false;
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
                mIsVideoSeeked = false;
            }else{
                packet_queue_put(&videoq, &pkt);
                mIsVideoSeeked = true;
                mIsVideoPlayed = false;
            }
        }else{
//            if(pkt.pts >= 0){
                packet_queue_put(&videoq, &pkt);
                mIsVideoPlayed = false;
//            }
        }


        if(++mVideoDecodedCount == 10 || (mVideoDecodedCount < 10 && pkt.flags == AV_PKT_FLAG_KEY)){
            if(mCallback){
                mCallback->mediaCanRenderFirstFrame();
            }
        }

        if(pkt.pts == AV_NOPTS_VALUE){
            mIsVideoLoadedCompleted = true;
        }else{
            mIsVideoLoadedCompleted = false;
        }
    }else if(pkt.stream_index == mAudioIndex && mIsOpenAudioCodec){
        mIsAudioLoadedCompleted = false;
        if(!mIsSeekd){
            qint64 audioTime = av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * pkt.pts * 1000;
            if(audioTime < mSeekTime){ //如果音频时间小于拖动的时间，则丢掉音频包
                av_packet_unref(&pkt);
                mIsAudioSeeked = false;
            }else{
                mIsAudioSeeked = true;
                mIsAudioPlayed = false;
                packet_queue_put(&audioq, &pkt);
            }
        }else{
            mIsAudioPlayed = false;
            packet_queue_put(&audioq, &pkt);
        }

        if(pkt.pts == AV_NOPTS_VALUE){
            mIsAudioLoadedCompleted = true;
        }else{
            mIsAudioLoadedCompleted = false;
        }
    }
    else if(mFormatCtx->streams[pkt.stream_index]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE){
        packet_queue_put(&subtitleq, &pkt);
        av_packet_unref(&pkt);
        mIsSubtitleSeeked = true;
        mIsSubtitlePlayed = false;
//        qDebug() << "------------------------------- AVMEDIA_TYPE_SUBTITLE";
//        qDebug() << "------------------------------- AVMEDIA_TYPE_SUBTITLE:" << pkt.stream_index << ":" << AVMEDIA_TYPE_SUBTITLE;
    }
    else if(mFormatCtx->streams[pkt.stream_index]->codec->codec_type == AVMEDIA_TYPE_DATA){
//        qDebug() << "------------------------------- AVMEDIA_TYPE_DATA";
        av_packet_unref(&pkt);
        mIsAudioLoadedCompleted = false;
        mIsVideoLoadedCompleted = false;
    }
    else{
        av_packet_unref(&pkt);
        mIsAudioLoadedCompleted = false;
        mIsVideoLoadedCompleted = false;
    }

    if((!mIsVideoSeeked && hasVideo()) || (!mIsAudioSeeked && hasAudio()) || (!mIsSubtitleSeeked && mHasSubtitle)){ //如果其中某一个元素未完成拖动，则继续解码s
        mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Decodec));
    }else{
        if(mMediaBufferMode == AVDefine::MediaBufferMode_Time){
            //判断是否将设定的缓存装满，如果未装满的话，一直循环
            int vstartTime = 0,vendTime = 0;
            if(mIsOpenVideoCodec){
                videoq.mutex.lock();
                vstartTime = videoq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * videoq.first_pkt->pkt.pts * 1000;
                vendTime = videoq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * videoq.last_pkt->pkt.pts * 1000;
                videoq.mutex.unlock();
            }

            int astartTime = 0,aendTime = 0;
            if(mIsOpenAudioCodec){
                audioq.mutex.lock();
                astartTime = audioq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * audioq.first_pkt->pkt.pts * 1000;
                aendTime = audioq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * audioq.last_pkt->pkt.pts * 1000;
                audioq.mutex.unlock();
            }

//            qDebug() << "---------" << (vendTime - vstartTime) << ":" << mIsOpenVideoCodec <<  ":" << mIsVideoLoadedCompleted;
            if(((vendTime - vstartTime < mBufferSize && mIsOpenVideoCodec && !mIsVideoLoadedCompleted) ||
                    (aendTime - astartTime < mBufferSize && mIsOpenAudioCodec && !mIsAudioLoadedCompleted))
                    && vendTime - vstartTime < mBufferSize && aendTime - astartTime < mBufferSize
                    ){//只要有一种流装满，则不继续处理
                mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Decodec));
            }else{
                videoq.mutex.lock();
                if(videoq.nb_packets == 0){
                    mIsVideoLoadedCompleted = true;
                }
                videoq.mutex.unlock();
                audioq.mutex.lock();
                if(audioq.nb_packets == 0){
                    mIsAudioLoadedCompleted = true;
                }
                audioq.mutex.unlock();
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
            if((videoq.nb_packets < mBufferSize && mIsOpenVideoCodec && !mIsVideoLoadedCompleted) || (audioq.nb_packets < mBufferSize && mIsOpenAudioCodec && !mIsAudioLoadedCompleted) )
                mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Decodec));
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

void AVDecoder::setFilename(const QString &source){
    mProcessThread.clearAllTask(); //清除所有任务
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_SetFileName,0,source));

}

void AVDecoder::setFilenameImpl(const QString &source){
    mProcessThread.clearAllTask(); //清除所有任务
    if(mFilename.size() > 0){
        packet_queue_flush(&audioq);
        packet_queue_flush(&videoq);
        packet_queue_flush(&subtitleq);
        memset(&audioq,0,sizeof(PacketQueue));
        memset(&videoq,0,sizeof(PacketQueue));
        memset(&subtitleq,0,sizeof(PacketQueue));
        release();//释放所有资源
    }
    mFilename = source;
    mIsInit = false;
    load();
}

void AVDecoder::setMediaCallback(AVMediaCallback *callback){
    this->mCallback = callback;
}

void AVDecoder::load(){
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Init));
}

/** 设置播放速率，最大为8，最小为1.0 / 8 */
void AVDecoder::setPlayRate(float rate){
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_SetPlayRate,rate));
}

float AVDecoder::getPlayRate(){
    return this->mPlayRate;
}

void AVDecoder::seek(int ms){
    mProcessThread.clearAllTask();
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Seek,ms));
}

void AVDecoder::setBufferSize(int size){
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_SetBufferSize,size));
}

void AVDecoder::setMediaBufferMode(AVDefine::MediaBufferMode mode){
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_SetMediaBufferMode,mode));
}

void AVDecoder::checkBuffer(){
    if(!mIsReadFinish){
        //判断是否将设定的缓存装满，如果未装满的话，一直循环
        //if(mIsOpenVideoCodec && !mIsVideoLoadedCompleted){
        if(mIsOpenVideoCodec){
            int vstartTime = videoq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * videoq.first_pkt->pkt.pts * 1000;
            int vendTime = videoq.nb_packets == 0 ? 0 : av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * videoq.last_pkt->pkt.pts * 1000;
            if(vendTime - vstartTime < mBufferSize){
                mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Decodec));
            }
        }

        //if(mIsOpenAudioCodec && !mIsAudioLoadedCompleted){
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
void AVDecoder::renderFirstFrame(){
    slotRenderFirstFrame();
    checkBuffer();
}

/** 渲染下一帧 */
void AVDecoder::renderNextFrame(){
    slotRenderNextFrame();
    checkBuffer();
}

/** 请求向音频buffer添加数据  */
void AVDecoder::requestAudioNextFrame(int len){
    slotRequestAudioNextFrame(len);
    checkBuffer();
}

/** video是否播放完成 */
bool AVDecoder::isVideoPlayed(){
    return mIsVideoPlayed;
}

bool AVDecoder::hasVideo(){
    return mIsOpenVideoCodec;
}

bool AVDecoder::hasAudio(){
    return mIsOpenAudioCodec;
}

void AVDecoder::slotSeek(int ms){
    if(ms == 0){
        int vstartTime = -1;
        if(hasVideo()){
            videoq.mutex.lock();
            vstartTime = videoq.nb_packets == 0 ? -1 : av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * videoq.first_pkt->pkt.pts * 1000;
            videoq.mutex.unlock();
        }

        int astartTime = -1;
        if(hasAudio()){
            audioq.mutex.lock();
            astartTime = audioq.nb_packets == 0 ? -1 : av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * audioq.first_pkt->pkt.pts * 1000;
            audioq.mutex.unlock();
        }

//        qDebug() << "---------------:" << astartTime << vstartTime;
        if(((vstartTime < 600 && vstartTime >= 0) || !hasVideo())
            && ((astartTime < 600 && astartTime >= 0) || !hasAudio())
                ){
//            qDebug() << "---------------:MediaStatus_Seeked";
            statusChanged(AVDefine::MediaStatus_Seeked);
            return;
        }

//        if((vstartTime == astartTime || !hasAudio()) && (vstartTime == ms && hasVideo())){
//            statusChanged(AVDefine::MediaStatus_Seeked);
//            return;
//        }
    }

    //清除队列

    packet_queue_flush(&audioq);
    packet_queue_flush(&videoq);
    packet_queue_flush(&subtitleq);
    mIsSeek = true;
    mSeekTime = ms;
    mIsReadFinish = false;
    mIsAudioLoadedCompleted = true;
    mIsVideoLoadedCompleted = true;
    mIsVideoPlayed = false;
    mIsAudioPlayed = false;
    checkBuffer();
}

void AVDecoder::slotSetBufferSize(int size){
    mBufferSize = size;
}

void AVDecoder::slotSetMediaBufferMode(AVDefine::MediaBufferMode mode){
    mMediaBufferMode = mode;
}

void AVDecoder::slotRenderFirstFrame(){
    slotRenderNextFrame();
}

void AVDecoder::slotSetPlayRate(float rate){
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



void AVDecoder::slotRenderNextFrame(){
    if(!mIsInit)
        return;
    if(!mIsOpenVideoCodec && !mIsOpenAudioCodec){
        statusChanged(AVDefine::MediaStatus_InvalidMedia);
        return;
    }
    if(mIsDestroy || mIsVideoPlayed)
        return;

    mVideoDecodecMutex.lock();
    AVPacket pkt;
    int isPacket = packet_queue_get(&videoq,&pkt);
    if(isPacket){
        mVideoCodecCtxMutex.lock();
        int ret = avcodec_send_packet(mVideoCodecCtx, &pkt);
        if(ret != 0){
            av_packet_unref(&pkt);
            mVideoCodecCtxMutex.unlock();
            mVideoDecodecMutex.unlock();
            return;
        }

        AVFrame *receiveFrame = mFrameIndex % 2 == 0 ? mFrame : mFrame1;
        vFormat.renderFrameMutex = mFrameIndex % 2 == 0 ? &mRenderFrameMutex : &mRenderFrameMutex2;

        vFormat.renderFrameMutex->lock();
        while(avcodec_receive_frame(mVideoCodecCtx, receiveFrame) == 0){
//            AVFrame *frame = receiveFrame;
            vFormat.renderFrame = receiveFrame;
            mHWFrame = NULL;
            #ifdef ENABLE_HW
            if (receiveFrame->format == mHWPixFormat && mIsSupportHw) {
                qDebug() << "----------------------------- 硬解。。。";
                mHWFrame = av_frame_alloc();
                /* retrieve data from GPU to CPU */
                if ((ret = av_hwframe_transfer_data(mHWFrame, mFrame, 0)) < 0) {
                    qDebug() << "Error transferring the data to system memory";
                }
//                frame = mHWFrame;
            }
            #endif

//            av_hwframe_transfer_data
//            av_hwframe_map()
//            AV_HWFRAME_MAP_DIRECT

//            if(mVideoSwsCtx != NULL){
//                ret = sws_scale(mVideoSwsCtx,
//                          receiveFrame->data,
//                          receiveFrame->linesize,
//                          0,
//                          receiveFrame->height,
//                          mFrameYUV->data,
//                          mFrameYUV->linesize);

//                qDebug() << "----------------------slotRenderNextFrame:"<< ret;
//                av_frame_unref(receiveFrame);
//                av_frame_copy(receiveFrame,mFrameYUV);
//                qDebug() << "----------------------slotRenderNextFrame:"<<receiveFrame->width<<mFrameYUV->width;
//                av_frame_unref(mFrameYUV);
//            }

            if(mHWFrame != NULL){
                av_frame_unref(mHWFrame);
            }

            if(mCallback){
                if(mHWFrame == NULL){
                    mCallback->mediaUpdateVideoFrame((void *)&vFormat);
                }
            }

            vFormat.renderFrameMutex->unlock();

            ++mFrameIndex;
            receiveFrame = mFrameIndex % 2 == 0 ? mFrame : mFrame1;
            vFormat.renderFrameMutex = mFrameIndex % 2 == 0 ? &mRenderFrameMutex : &mRenderFrameMutex2;
            av_frame_unref(receiveFrame);

            vFormat.renderFrameMutex->lock();
        }
        vFormat.renderFrameMutex->unlock();

        mVideoCodecCtxMutex.unlock();
    }else{
        if(!mIsReadFinish && !mIsVideoLoadedCompleted){
            statusChanged(AVDefine::MediaStatus_Buffering);
        }
        else{
            mIsVideoPlayed = true;
        }
        if(mIsVideoPlayed && mIsAudioPlayed && mIsSubtitlePlayed && mIsReadFinish){
            emit statusChanged(AVDefine::MediaStatus_Played);
        }
    }
    if(isPacket)
        av_packet_unref(&pkt);
    mVideoDecodecMutex.unlock();
}

void AVDecoder::slotRequestAudioNextFrame(int len){
    if(!mIsInit)
        return;

    if(!mIsOpenVideoCodec && !mIsOpenAudioCodec){
        statusChanged(AVDefine::MediaStatus_InvalidMedia);
        return;
    }

    if(mIsDestroy || mIsAudioPlayed)
        return;

    mAudioBufferMutex.lock();
    int audioBufferLen = mAudioBuffer.length();
    mAudioBufferMutex.unlock();
    if(audioBufferLen < len){
        AVPacket pkt;
        int isPacket = packet_queue_get(&audioq,&pkt);
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

                delete mAudioDstData;
                mAudioDstData = NULL;
            }
            slotRequestAudioNextFrame(len);
        }else{
            if(!mIsReadFinish && !mIsAudioLoadedCompleted)
                statusChanged(AVDefine::MediaStatus_Buffering);
            else{
                mIsAudioPlayed = true;
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

    if(mIsVideoPlayed && mIsAudioPlayed && mIsSubtitlePlayed && mIsReadFinish){
        emit statusChanged(AVDefine::MediaStatus_Played);
    }
}

int AVDecoder::getCurrentVideoTime(){
    int time = mIsReadFinish ? -1 : 0;
    if(hasVideo()){
        videoq.mutex.lock();
        if(videoq.nb_packets > 0){
            time = av_q2d(mFormatCtx->streams[mVideoIndex]->time_base) * videoq.first_pkt->pkt.pts * 1000;
        }
        videoq.mutex.unlock();
    }
    return time;
}

void AVDecoder::packet_queue_init(PacketQueue *q) {
//    memset(q, 0, sizeof(PacketQueue));
//    q->mutex = new QMutex;

    q->mutex.lock();
    q->size = 0;
    q->nb_packets = 0;
    q->first_pkt = NULL;
    q->last_pkt = NULL;
    q->isInit = true;
    q->time = 0;
    q->mutex.unlock();
}

int AVDecoder::packet_queue_put(PacketQueue *q, AVPacket *pkt) {
    AVPacketList *pkt1;
    if (av_dup_packet(pkt) < 0) {
        return -1;
    }
    pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    q->mutex.lock();

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;

    q->mutex.unlock();
    return 0;
}

int AVDecoder::packet_queue_get(PacketQueue *q, AVPacket *pkt) {
    AVPacketList *pkt1;
    int ret;

    q->mutex.lock();
    if(q->nb_packets == 0)
    {
        q->mutex.unlock();
        return 0;
    }
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
        } else{
            ret = 0;
            break;
        }
    }
    q->mutex.unlock();
    return ret;
}

void AVDecoder::packet_queue_flush(PacketQueue *q)
{
    q->mutex.lock();
    AVPacketList *pkt, *pkt1;
    if(q->nb_packets == 0 || !q->isInit){
        q->mutex.unlock();
        return;
    }

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
    q->time = 0;
    q->mutex.unlock();
}

void AVDecoder::packet_queue_destroy(PacketQueue *q)
{
    q->mutex.lock();
    bool isEnter  = false;
    if(q->isInit){
        isEnter = true;
        q->mutex.unlock();

        packet_queue_flush(q);

        q->mutex.lock();
        q->isInit = false;
        q->mutex.unlock();
    }

    if(!isEnter)
        q->mutex.unlock();
}

void AVDecoder::statusChanged(AVDefine::MediaStatus status){
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
    case AVCodecTaskCommand_SetFileName:{
        mCodec->setFilenameImpl(param2);
    }
    }
}
