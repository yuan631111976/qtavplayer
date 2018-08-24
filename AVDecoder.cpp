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

/******************************************
   *
   * Convert 32 bit float to a 4 byte array
   */
static unsigned char * FloatToByteArray(float f)
{
    static unsigned char p[4];
    unsigned char * x = reinterpret_cast<unsigned char *>(&f);
    for (int i = 0; i < 4; ++i)
    {
        p[i] = x[i];
    }
    return p;
}

static void FloatArrayToByteArray(float f[], int float_count, unsigned char byte_array[])
{
    for(int i = 0; i < float_count; i++)
    {
        unsigned char * x = reinterpret_cast<unsigned char *>(&f[i]);
        for (int j = 0; j < 4; ++j)
        {
            byte_array[4 * i + j] = x[j];
        }
    }
}

static float ByteArrayToFloat(unsigned char foo[])
{
    union test
    {
        unsigned char buf[4];
        float number;
    }test;

    test.buf[0] = foo[0];
    test.buf[1] = foo[1];
    test.buf[2] = foo[2];
    test.buf[3] = foo[3];
    return test.number;
}

static void ByteArrayToFloatArray(unsigned char byte_array[], int byte_count, float float_array[])
{
    union test
    {
        unsigned char buf[4];
        float number;
    }test;

    for(int i=0;i < (byte_count / 4); i++)
    {
        test.buf[0] = byte_array[ i * 4 + 0];
        test.buf[1] = byte_array[ i * 4 + 1];
        test.buf[2] = byte_array[ i * 4 + 2];
        test.buf[3] = byte_array[ i * 4 + 3];
        float_array[i] = test.number;
    }
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

static int lockmgr(void **mtx, enum AVLockOp op)
{
   switch(op) {
      case AV_LOCK_CREATE:{
           QMutex *mutex = new QMutex;
           *mtx = mutex;
           return 0;
      }
      case AV_LOCK_OBTAIN:{
           QMutex *mutex = (QMutex*)*mtx;
           mutex->lock();
           return 0;
       }
      case AV_LOCK_RELEASE:{
           QMutex *mutex = (QMutex*)*mtx;
           mutex->unlock();
           return 0;
      }
      case AV_LOCK_DESTROY:{
           QMutex *mutex = (QMutex*)*mtx;
           delete mutex;
           return 0;
      }
   }
   return 1;
}

AVDecoder::AVDecoder()
    : mAudioIndex(-1)
    , mVideoIndex(-1)
    , mFormatCtx(NULL)
    , mAudioCodecCtx(NULL)
    , mVideoCodecCtx(NULL)
    , mAudioCodec(NULL)
    , mVideoCodec(NULL)
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
    , reciveFrame(NULL)
    , mIsInit(false)
    , mIsSupportHw(false)
    , mHWDeviceCtx(NULL)
    , mIsEnableHwDecode(false)
    , mFrameIndex(0)
    , mIsVideoLoadedCompleted(true)
    , mIsAudioLoadedCompleted(true)
    , maxRenderListSize(10)
    , mLastRenderItem(NULL)
    , mIsNeedCallRenderFirstFrame(true)
    , mIsAccompany(false)
{
#ifdef LIBAVUTIL_VERSION_MAJOR
#if (LIBAVUTIL_VERSION_MAJOR < 56)
    avcodec_register_all();
    avfilter_register_all();
    av_register_all();
    avformat_network_init();
#endif
#endif

    av_log_set_callback(NULL);//不打印日志
    av_lockmgr_register(lockmgr);


    audioq.release();
    videoq.release();
    subtitleq.release();
    initRenderList();
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

    if(!getpreview()){
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
                mAudioCodecCtx->thread_count = 0;
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
    }

    //    mIsOpenAudioCodec = false;
    //    mIsOpenVideoCodec = false;
    if(!mIsOpenVideoCodec && !mIsOpenAudioCodec){ //如果即没有音频，也没有视频，则通知状态无效
        statusChanged(AVDefine::MediaStatus_InvalidMedia);
        release();//释放所有资源
        return;
    }

    if(mIsOpenVideoCodec){
        AVDictionaryEntry *tag = NULL;
        tag = av_dict_get(mFormatCtx->streams[mVideoIndex]->metadata, "rotate", tag, 0);
        if(tag != NULL)
            mRotate = QString(tag->value).toInt();
        av_free(tag);

        vFormat.width = mVideoCodecCtx->width;
        vFormat.height = mVideoCodecCtx->height;
        vFormat.rotate = mRotate;
        vFormat.format = mVideoCodecCtx->pix_fmt;


        //        qDebug() <<"----------------------- : " << mVideoCodecCtx->pix_fmt << ":" << AV_PIX_FMT_BGR24;
        switch (mVideoCodecCtx->pix_fmt) {
        case AV_PIX_FMT_YUV420P :
        case AV_PIX_FMT_YUVJ420P :
        case AV_PIX_FMT_YUV422P :
        case AV_PIX_FMT_YUVJ422P :
        case AV_PIX_FMT_YUV444P :
        case AV_PIX_FMT_YUVJ444P :
        case AV_PIX_FMT_GRAY8 :
        case AV_PIX_FMT_UYVY422 :
        case AV_PIX_FMT_YUYV422 :
        case AV_PIX_FMT_YUV420P10LE :
        case AV_PIX_FMT_BGR24 :
        case AV_PIX_FMT_RGB24 :
        case AV_PIX_FMT_YUV410P :
        case AV_PIX_FMT_YUV411P :
        case AV_PIX_FMT_MONOWHITE :
        case AV_PIX_FMT_MONOBLACK :
        case AV_PIX_FMT_PAL8 :
        case AV_PIX_FMT_UYYVYY411 :
        case AV_PIX_FMT_BGR8 :
        case AV_PIX_FMT_RGB8 :
//        case AV_PIX_FMT_BGR4 :
//        case AV_PIX_FMT_BGR4_BYTE :
//        case AV_PIX_FMT_RGB4 :
//        case AV_PIX_FMT_RGB4_BYTE :
        case AV_PIX_FMT_NV12 :
        case AV_PIX_FMT_NV21 :
        case AV_PIX_FMT_ARGB :
        case AV_PIX_FMT_RGBA :
        case AV_PIX_FMT_ABGR :
        case AV_PIX_FMT_BGRA :
//        case AV_PIX_FMT_GRAY16BE :
        case AV_PIX_FMT_GRAY16LE :
        case AV_PIX_FMT_YUV440P :
        case AV_PIX_FMT_YUVJ440P :
        case AV_PIX_FMT_YUVA420P :

//        case AV_PIX_FMT_RGB444LE :
//        case AV_PIX_FMT_RGB444BE :
//        case AV_PIX_FMT_BGR444LE :
//        case AV_PIX_FMT_BGR444BE :

         case AV_PIX_FMT_YUV420P16LE :
//         case AV_PIX_FMT_YUV420P16BE :
         case AV_PIX_FMT_YUV422P16LE :
//         case AV_PIX_FMT_YUV422P16BE :
         case AV_PIX_FMT_YUV444P16LE :
//         case AV_PIX_FMT_YUV444P16BE :

//        case AV_PIX_FMT_YUVA420P16BE :
        case AV_PIX_FMT_YUVA420P16LE :
//        case AV_PIX_FMT_YUVA422P16BE :
        case AV_PIX_FMT_YUVA422P16LE :
//        case AV_PIX_FMT_YUVA444P16BE :
        case AV_PIX_FMT_YUVA444P16LE :

//        case AV_PIX_FMT_BAYER_BGGR16LE :
//        case AV_PIX_FMT_BAYER_BGGR16BE :
//        case AV_PIX_FMT_BAYER_RGGB16LE :
//        case AV_PIX_FMT_BAYER_RGGB16BE :
//        case AV_PIX_FMT_BAYER_GBRG16LE :
//        case AV_PIX_FMT_BAYER_GBRG16BE :
//        case AV_PIX_FMT_BAYER_GRBG16LE :
//        case AV_PIX_FMT_BAYER_GRBG16BE :

        case AV_PIX_FMT_YUV444P10LE :
            break;
        default: //AV_PIX_FMT_YUV420P  如果上面的格式不支持直接渲染，则转换成通用AV_PIX_FMT_YUV420P格式
            vFormat.format = AV_PIX_FMT_YUV420P;
            mVideoSwsCtx = sws_getContext(
                        mVideoCodecCtx->width,
                        mVideoCodecCtx->height,
                        mVideoCodecCtx->pix_fmt,
                        mVideoCodecCtx->width,
                        mVideoCodecCtx->height,
                        (AVPixelFormat)vFormat.format,
                        SWS_BICUBIC,NULL,NULL,NULL);
            reciveFrame = av_frame_alloc();
            changeRenderItemSize(mVideoCodecCtx->width,mVideoCodecCtx->height,(AVPixelFormat)vFormat.format);
            break;
        }
        videoq.setTimeBase(mFormatCtx->streams[mVideoIndex]->time_base);
        videoq.init();
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

    audioq.setTimeBase(mFormatCtx->streams[mAudioIndex]->time_base);
    audioq.init();

    mIsInit = true;
    statusChanged(AVDefine::MediaStatus_Buffering);
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Decodec));
}

/** 设置是否启用硬用 */
void AVDecoder::setHWDecodec(bool enable){

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
    //    qDebug() << "-----------------8";
    if(mAudioFrame != NULL){
        av_frame_unref(mAudioFrame);
        av_frame_free(&mAudioFrame);
        mAudioFrame = NULL;
    }
    //    qDebug() << "-----------------9";
    if(reciveFrame != NULL){
        av_frame_unref(reciveFrame);
        av_frame_free(&reciveFrame);
        reciveFrame = NULL;
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
    audioq.release();
    videoq.release();
    subtitleq.release();
    clearRenderList(isDeleted);
}

void AVDecoder::showFrameByPositionImpl(int time){
    AVFormatContext *formatCtx = NULL;
    AVCodec *videoCodec = NULL;
    AVCodecContext *videoCodecCtx = NULL;
    int videoIndex = -1;
    int rotate = 0;
    struct SwsContext *videoSwsCtx = NULL; //视频参数转换上下文

    if(avformat_open_input(&formatCtx, mFilename.toStdString().c_str(), NULL, NULL) != 0)
    {
        return;
    }

    if(avformat_find_stream_info(formatCtx, NULL) < 0)
    {
        return;
    }

    /* 寻找视频流 */
    int ret = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &videoCodec, 0);
    if (ret >= 0){
        videoIndex = ret;
        videoCodecCtx = avcodec_alloc_context3(videoCodec);
        if (!videoCodecCtx){

        }else{
            avcodec_parameters_to_context(videoCodecCtx, formatCtx->streams[videoIndex]->codecpar);
            videoCodecCtx->thread_count = 0;
            if(avcodec_open2(videoCodecCtx, videoCodec, NULL) < 0)
            {
                AVDictionaryEntry *tag = NULL;
                tag = av_dict_get(formatCtx->streams[videoIndex]->metadata, "rotate", tag, 0);
                if(tag != NULL)
                    rotate = QString(tag->value).toInt();
                switch (videoCodecCtx->pix_fmt) {
                case AV_PIX_FMT_YUV420P :
                case AV_PIX_FMT_YUVJ420P :
                case AV_PIX_FMT_YUV422P :
                case AV_PIX_FMT_YUVJ422P :
                case AV_PIX_FMT_YUV444P :
                case AV_PIX_FMT_YUVJ444P :
                case AV_PIX_FMT_GRAY8 :
                case AV_PIX_FMT_UYVY422 :
                case AV_PIX_FMT_YUYV422 :
                case AV_PIX_FMT_YUV420P10LE :
                case AV_PIX_FMT_BGR24 :
                case AV_PIX_FMT_RGB24 :
                case AV_PIX_FMT_YUV410P :
                case AV_PIX_FMT_YUV411P :
                case AV_PIX_FMT_MONOWHITE :
                case AV_PIX_FMT_MONOBLACK :
                case AV_PIX_FMT_PAL8 :
                case AV_PIX_FMT_UYYVYY411 :
                case AV_PIX_FMT_BGR8 :
                case AV_PIX_FMT_RGB8 :
                case AV_PIX_FMT_NV12 :
                case AV_PIX_FMT_NV21 :
                case AV_PIX_FMT_ARGB :
                case AV_PIX_FMT_RGBA :
                case AV_PIX_FMT_ABGR :
                case AV_PIX_FMT_BGRA :
                case AV_PIX_FMT_GRAY16LE :
                case AV_PIX_FMT_YUV440P :
                case AV_PIX_FMT_YUVJ440P :
                case AV_PIX_FMT_YUVA420P :
                case AV_PIX_FMT_YUV420P16LE :
                case AV_PIX_FMT_YUV422P16LE :
                case AV_PIX_FMT_YUV444P16LE :
                case AV_PIX_FMT_YUVA420P16LE :
                case AV_PIX_FMT_YUVA422P16LE :
                case AV_PIX_FMT_YUVA444P16LE :
                case AV_PIX_FMT_YUV444P10LE :
                    break;
                default: //AV_PIX_FMT_YUV420P  如果上面的格式不支持直接渲染，则转换成通用AV_PIX_FMT_YUV420P格式
                    videoSwsCtx = sws_getContext(
                                videoCodecCtx->width,
                                videoCodecCtx->height,
                                videoCodecCtx->pix_fmt,
                                videoCodecCtx->width,
                                videoCodecCtx->height,
                                AV_PIX_FMT_YUV420P,
                                SWS_BICUBIC,NULL,NULL,NULL);
                    break;
                }



                av_seek_frame(formatCtx,mVideoIndex,time * 1000,0);
                AVPacket *pkt = av_packet_alloc();
                while(av_read_frame(formatCtx, pkt) >= 0){
                    if(pkt->stream_index == videoIndex){ //视频
                        AVFrame *tempFrame = av_frame_alloc();
                        while(avcodec_receive_frame(mVideoCodecCtx, tempFrame) == 0){
                            av_frame_unref(tempFrame);
                        }
                        av_frame_free(&tempFrame);

                        av_packet_unref(pkt);
                        break;
                    }
                    av_packet_unref(pkt);
                }
                av_packet_free(&pkt);
            }
        }
    }else{

    }
}

void AVDecoder::decodec(){
    if((!mIsOpenVideoCodec && !mIsOpenAudioCodec) || !mIsInit){//必须同时存在音频和视频才能播放
        statusChanged(AVDefine::MediaStatus_InvalidMedia);
        return;
    }

    if(mIsDestroy || mIsReadFinish)
        return;

    if(mIsSeek){
//                qDebug() <<"------------- seek";
        statusChanged(AVDefine::MediaStatus_Seeking);
        av_seek_frame(mFormatCtx,-1,mSeekTime * 1000,AVSEEK_FLAG_BACKWARD);
        //        qDebug() << "------------------- av_seek_frame:" << mVideoCodecCtx->pts_correction_last_pts;
        mIsSeek = false;
        //        mIsVideoSeeked = false;
        //        mIsAudioSeeked = false;
        if(mHasSubtitle)
            mIsSubtitleSeeked = false;
        mIsSeekd = false;
    }

    mIsVideoSeeked = true;
    mIsAudioSeeked = true;

    AVPacket *pkt = av_packet_alloc();
    int ret = av_read_frame(mFormatCtx, pkt);

    mIsReadFinish = ret < 0;
    if(mIsReadFinish){
//        qDebug() << "--------------------- decoded completed";
        if(!mIsSeekd){
            mIsSeekd = true;
            //            if(mCallback){
            //                slotRenderNextFrame();
            //                mCallback->mediaCanRenderFirstFrame();
            //            }
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

    //    qDebug() << "------------------------------- decodec:" << pkt.stream_index << ":" << mFormatCtx->streams[pkt.stream_index]->codec->codec_type;
    if (pkt->stream_index == mVideoIndex && mIsOpenVideoCodec){
        //        qDebug() <<"------------- video";
        mIsVideoLoadedCompleted = false;
        if(!mIsSeekd){
            int currentTime = av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * pkt->pts * 1000;
            if(currentTime < mSeekTime){
                mVideoCodecCtxMutex.lock();
                ret = avcodec_send_packet(mVideoCodecCtx, pkt);
                if(ret != 0){
                }else{
                    AVFrame *tempFrame = av_frame_alloc();
                    while(avcodec_receive_frame(mVideoCodecCtx, tempFrame) == 0){
                        av_frame_unref(tempFrame);
                    }
                    av_frame_free(&tempFrame);
                }
                av_packet_unref(pkt);
                av_freep(pkt);
                pkt = NULL;
                mVideoCodecCtxMutex.unlock();
                mIsVideoSeeked = false;
            }else{
                videoq.put(pkt);
                mIsVideoSeeked = true;
                mIsVideoPlayed = false;
            }
        }else{
            videoq.put(pkt);
            mIsVideoPlayed = false;
        }
        if(pkt != NULL && pkt->pts == AV_NOPTS_VALUE){
            mIsVideoLoadedCompleted = true;
        }else{
            mIsVideoLoadedCompleted = false;
        }
    }else if(pkt->stream_index == mAudioIndex && mIsOpenAudioCodec && !getpreview()){ //预览时，不处理音频
        mIsAudioLoadedCompleted = false;
        if(!mIsSeekd){
            qint64 audioTime = av_q2d(mFormatCtx->streams[mAudioIndex]->time_base ) * pkt->pts * 1000;
            if(audioTime < mSeekTime){ //如果音频时间小于拖动的时间，则丢掉音频包
                av_packet_unref(pkt);
                av_freep(pkt);
                pkt = NULL;
                mIsAudioSeeked = false;
            }else{
                mIsAudioSeeked = true;
                mIsAudioPlayed = false;
                audioq.put(pkt);
            }
        }else{
            mIsAudioPlayed = false;
            audioq.put(pkt);
        }

        if(pkt != NULL && pkt->pts == AV_NOPTS_VALUE){
            mIsAudioLoadedCompleted = true;
        }else{
            mIsAudioLoadedCompleted = false;
        }
    }
    else if(mFormatCtx->streams[pkt->stream_index]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE){
        //        subtitleq.put(&pkt);
        /*
        if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_DVD_SUBTITLE) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_DVD_SUBTITLE");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_DVB_SUBTITLE) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_DVB_SUBTITLE");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_TEXT) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_TEXT");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_XSUB) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_XSUB");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_SSA) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_SSA");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_MOV_TEXT) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_MOV_TEXT");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_HDMV_PGS_SUBTITLE) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_HDMV_PGS_SUBTITLE");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_DVB_TELETEXT) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_DVB_TELETEXT");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_SRT) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_SRT");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_MICRODVD) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_MICRODVD");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_EIA_608) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_EIA_608");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_JACOSUB) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_JACOSUB");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_SAMI) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_SAMI");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_REALTEXT) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_REALTEXT");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_SUBVIEWER1) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_SUBVIEWER1");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_SUBVIEWER) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_SUBVIEWER");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_SUBRIP) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_SUBRIP");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_WEBVTT) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_WEBVTT");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_MPL2) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_MPL2");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_VPLAYER) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_VPLAYER");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_PJS) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_PJS");
        } else if (mFormatCtx->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_ASS) {
            qDebug("[nativeGetContentInfo] AVMEDIA_TYPE_SUBTITLE = AV_CODEC_ID_ASS");
        }else{
            qDebug("AVMEDIA_TYPE_SUBTITLE");
        }

        qDebug("AVMEDIA_TYPE_SUBTITLE");
        qDebug() << "AVMEDIA_TYPE_SUBTITLE 222";*/

        av_packet_unref(pkt);
        av_freep(pkt);
        pkt = NULL;
        mIsSubtitleSeeked = true;
//        mIsSubtitlePlayed = false;
//                qDebug() << "------------------------------- AVMEDIA_TYPE_SUBTITLE";
        //        qDebug() << "------------------------------- AVMEDIA_TYPE_SUBTITLE:" << pkt.stream_index << ":" << AVMEDIA_TYPE_SUBTITLE;
    }
    else if(mFormatCtx->streams[pkt->stream_index]->codec->codec_type == AVMEDIA_TYPE_DATA){
//                qDebug() << "------------------------------- AVMEDIA_TYPE_DATA";
        av_packet_unref(pkt);
        av_freep(pkt);
        pkt = NULL;
        mIsAudioLoadedCompleted = false;
        mIsVideoLoadedCompleted = false;
    }
    else{
        av_packet_unref(pkt);
        av_freep(pkt);
        pkt = NULL;
        mIsAudioLoadedCompleted = false;
        mIsVideoLoadedCompleted = false;
    }

    if((!mIsVideoSeeked && hasVideo()) || (!mIsAudioSeeked && hasAudio()) || (!mIsSubtitleSeeked && mHasSubtitle)){ //如果其中某一个元素未完成拖动，则继续解码s
        mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Decodec));
    }else{
        //判断是否将设定的缓存装满，如果未装满的话，一直循环
        int videoDiffTime = videoq.diffTime(),audioDiffTime = audioq.diffTime();
        if(((videoDiffTime < mBufferSize && mIsOpenVideoCodec && !mIsVideoLoadedCompleted) ||
            (audioDiffTime < mBufferSize && mIsOpenAudioCodec && !mIsAudioLoadedCompleted))
                && videoDiffTime < mBufferSize && audioDiffTime < mBufferSize
                ){//只要有一种流装满，则不继续处理
            mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Decodec));
        }else{
            if(videoq.size() == 0){
                mIsVideoLoadedCompleted = true;
            }

            if(audioq.size() == 0){
                mIsAudioLoadedCompleted = true;
            }

            if(!mIsSeekd){
                mIsSeekd = true;
                statusChanged(AVDefine::MediaStatus_Buffered);
                statusChanged(AVDefine::MediaStatus_Seeked);
            }
            if(mStatus == AVDefine::MediaStatus_Buffering){
                statusChanged(AVDefine::MediaStatus_Buffered);
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
        clearRenderList();
        release();//释放所有资源
    }
    mFilename = source;
    mIsInit = false;
    mIsNeedCallRenderFirstFrame = true;
    load();
}

void AVDecoder::stop(){
    mAudioBufferMutex.lock();
    //清除音频buffer
    mAudioBuffer.clear();
    mAudioBufferMutex.unlock();
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


bool AVDecoder::getAccompany()const{
    return mIsAccompany;
}
void AVDecoder::setAccompany(bool flag){
    mIsAccompany = flag;
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
            if(videoq.diffTime() < mBufferSize){
                mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Decodec));
            }
        }

        //if(mIsOpenAudioCodec && !mIsAudioLoadedCompleted){
        if(mIsOpenAudioCodec){
            if(audioq.diffTime() < mBufferSize ){
                mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Decodec));
            }
        }
    }
}

void AVDecoder::releseCurrentRenderItem(){
    if(mLastRenderItem != NULL && mLastRenderItem->isRendered && mLastRenderItem->valid){
        mLastRenderItem->mutex.lock();
        mVideoSwsCtxMutex.lock();
        mLastRenderItem->release(mVideoSwsCtx == NULL);
        mVideoSwsCtxMutex.unlock();
        mLastRenderItem->mutex.unlock();
    }
}

void AVDecoder::resetVideoCodecContext(){
    bool locked = mVideoCodecCtxMutex.tryLock();
    if(mVideoCodecCtx != NULL){
        if(avcodec_is_open(mVideoCodecCtx)){
            avcodec_close(mVideoCodecCtx);
            if(avcodec_open2(mVideoCodecCtx, mVideoCodec, NULL) < 0)
            {
                mIsOpenVideoCodec = false;
            }
        }
    }
    if(locked)
        mVideoCodecCtxMutex.unlock();
}

/** 渲染第一帧 */
void AVDecoder::renderFirstFrame(){
    mRenderListMutex.lock();
    int minTime = -1;
    RenderItem *render = NULL;
    for(int i = 0,len = mRenderList.size();i < len;i++){
        RenderItem *item = mRenderList[i];
        item->mutex.lock();
        if(item->valid){
            if(minTime == -1){
                minTime = item->pts;
            }
            if(item->pts <= minTime){
                minTime = item->pts;
                render = item;
            }
        }
        item->mutex.unlock();
    }
    if(render != NULL){
        if(mCallback){
            if(mHWFrame == NULL){
                releseCurrentRenderItem();
                vFormat.renderFrame = render->frame;
                vFormat.renderFrameMutex = &render->mutex;
                render->isRendered = true;
                mCallback->mediaUpdateVideoFrame((void *)&vFormat);
                mLastRenderItem = render;
            }
        }
    }
    mRenderListMutex.unlock();
}

/** 渲染下一帧 */
void AVDecoder::checkRenderList(){
    if(mIsDestroy)
        return;
    int size = getRenderListSize();
    int packet_size = videoq.size();
    if(size < maxRenderListSize &&  packet_size > 0){
        slotRenderNextFrame();
    }

    if(size < maxRenderListSize && videoq.size() > size){
        mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_DecodeToRender));
    }
}

/** 显示指定位置的帧 */
void AVDecoder::showFrameByPosition(int time){
    mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_ShowFrameByPosition,time));
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
            vstartTime = videoq.startTime();
        }

        int astartTime = -1;
        if(hasAudio()){
            astartTime = audioq.startTime();
        }

        //        qDebug() << "---------------:" << astartTime << vstartTime;
        if(((vstartTime < 1000 && vstartTime >= 0) || !hasVideo())
                && ((astartTime < 1000 && astartTime >= 0) || !hasAudio())
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
    resetVideoCodecContext();
    clearRenderList();
    audioq.release();
    videoq.release();
    subtitleq.release();
    mIsSeek = true;
    mSeekTime = ms;
    mIsReadFinish = false;
    mIsAudioLoadedCompleted = true;
    mIsVideoLoadedCompleted = true;
    mIsVideoPlayed = false;
    mIsAudioPlayed = false;
    mIsNeedCallRenderFirstFrame = true;
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

    if(getRenderListSize() == maxRenderListSize) //如果渲染队列已经装满，则不处理
        return;

    mRenderListMutex.lock();
    mVideoDecodecMutex.lock();

    AVPacket *pkt = videoq.get();
    if(pkt){
        mVideoCodecCtxMutex.lock();
        int ret = avcodec_send_packet(mVideoCodecCtx, pkt);
        if(ret != 0){
            av_packet_unref(pkt);
            av_freep(pkt);
            mVideoCodecCtxMutex.unlock();
            mVideoDecodecMutex.unlock();
            mRenderListMutex.unlock();
            return;
        }
        /*
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
                // retrieve data from GPU to CPU
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
        vFormat.renderFrameMutex->unlock();*/

        RenderItem *render = getInvalidRenderItem();
        render->mutex.lock();

        AVFrame *frame = render->frame;
        if(mVideoSwsCtx != NULL){
            frame = reciveFrame;
        }

        while(avcodec_receive_frame(mVideoCodecCtx, frame) == 0){
            if(mVideoSwsCtx != NULL){ //将不支持的格式进行转换
                ret = sws_scale(mVideoSwsCtx,
                                frame->data,
                                frame->linesize,
                                0,
                                frame->height,

                                render->frame->data,
                                render->frame->linesize);

                render->frame->pts = frame->pts;
                render->frame->width = frame->width;
                render->frame->height = frame->height;
                av_frame_unref(frame);
            }

            if(render->frame->pts != AV_NOPTS_VALUE){
                render->pts = av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * render->frame->pts * 1000;

                //                qDebug() <<  mVideoCodecCtx->pts_correction_last_pts << mVideoCodecCtx->pts_correction_last_dts << mVideoCodecCtx->frame_number;
                //                qDebug() << "-----------render->pts : " << render->pts << pkt->pts << render->frame->pts;
                render->valid = true;
                render->isRendered = false;
//                qDebug() << "---------------- pts : " << render->pts;
            }else{
                mVideoSwsCtxMutex.lock();
                render->release(mVideoSwsCtx == NULL);
                mVideoSwsCtxMutex.unlock();
            }

            render->mutex.unlock();


            render = getInvalidRenderItem();
            if(render != NULL){
                render->mutex.lock();
                if(mVideoSwsCtx == NULL)
                    frame = render->frame;
            }
            else
                break;
        }

        if(mVideoSwsCtx != NULL){
            av_frame_unref(frame);
        }

        if(render != NULL)
            render->mutex.unlock();

        mVideoCodecCtxMutex.unlock();
    }else{
        if(!mIsReadFinish && !mIsVideoLoadedCompleted){
            statusChanged(AVDefine::MediaStatus_Buffering);
        }
        else if(getRenderListSize() <= 1 && videoq.size() == 0){
            mIsVideoPlayed = true;
        }
        if(mIsVideoPlayed && mIsAudioPlayed && mIsSubtitlePlayed && mIsReadFinish){
            //            qDebug() << "--------------- 播放完成B";
            emit statusChanged(AVDefine::MediaStatus_Played);
        }
    }
    if(pkt){
        av_packet_unref(pkt);
        av_freep(pkt);
    }
    mVideoDecodecMutex.unlock();
    mRenderListMutex.unlock();

    if(getRenderListSize() == 1 && mIsNeedCallRenderFirstFrame){
        mIsNeedCallRenderFirstFrame = false;
        if(mCallback){
            mCallback->mediaCanRenderFirstFrame();
        }
    }
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
        AVPacket *pkt = audioq.get();
        if(pkt){

            int ret = avcodec_send_packet(mAudioCodecCtx, pkt);
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
            else if(audioq.size() == 0){
                mIsAudioPlayed = true;
            }
        }

        if(pkt){
            av_packet_unref(pkt);
            av_freep(pkt);
        }

    }else{
        mAudioBufferMutex.lock();
        QByteArray r = mAudioBuffer.left(len);
        mAudioBuffer.remove(0,len);
        mAudioBufferMutex.unlock();
        if(mCallback){
            if(mIsAccompany){ //伴唱
                float *floatArray = new float[r.size() >> 2];
                float *outFloatArray = new float[r.size() >> 2];
                ByteArrayToFloatArray((uint8_t*)r.data(),r.size(),floatArray);
                AVAudioEffect::removeVoice(floatArray,outFloatArray,r.size() >> 2,mSourceAudioFormat.channelCount());
                FloatArrayToByteArray(outFloatArray,r.size() >> 2,(uint8_t*)r.data());
                delete [] floatArray;
                delete [] outFloatArray;
            }

//            float *floatArray = new float[r.size() >> 2];
//            float *outFloatArray = new float[r.size() >> 2];
//            ByteArrayToFloatArray((uint8_t*)r.data(),r.size(),floatArray);
//            AVAudioEffect::girl(floatArray,r.size() >> 2,outFloatArray,mSourceAudioFormat.sampleRate());
//            FloatArrayToByteArray(outFloatArray,r.size() >> 2,(uint8_t*)r.data());
//            delete [] floatArray;
//            delete [] outFloatArray;

            mCallback->mediaUpdateAudioFrame(r);
        }
    }

    if(mIsVideoPlayed && mIsAudioPlayed && mIsSubtitlePlayed && mIsReadFinish){
        //        qDebug() << "--------------- 播放完成C:" << audioq.size() << videoq.size() << getRenderListSize();
        emit statusChanged(AVDefine::MediaStatus_Played);
    }
}

int AVDecoder::requestRenderNextFrame(){
    int time = mIsReadFinish ? -1 : 0;
    if(hasVideo()){
        checkBuffer();
        mRenderListMutex.lock();
        int minTime = time;
        RenderItem *render = NULL;
        for(int i = 0,len = mRenderList.size();i < len;i++){
            RenderItem *item = mRenderList[i];
            if(item->isRendered){
                continue;
            }
            item->mutex.lock();
            if(item->valid){
                if(minTime == time){
                    minTime = item->pts;
                }

                if(item->pts <= minTime){
                    minTime = item->pts;
                    render = item;
                }
            }
            item->mutex.unlock();
        }
        //        qDebug() << "------------- "<< render;
        if(render != NULL){
            if(mCallback){
                if(mHWFrame == NULL){
                    if(mLastRenderItem != NULL && mLastRenderItem->isRendered && mLastRenderItem->valid){
                        mLastRenderItem->mutex.lock();
                        mVideoSwsCtxMutex.lock();
                        mLastRenderItem->release(mVideoSwsCtx == NULL);
                        mVideoSwsCtxMutex.unlock();
                        mLastRenderItem->mutex.unlock();
                    }
                    vFormat.renderFrame = render->frame;
                    vFormat.renderFrameMutex = &render->mutex;
                    render->isRendered = true;
                    mCallback->mediaUpdateVideoFrame((void *)&vFormat);

                    mLastRenderItem = render;
                }
            }
        }
        time = minTime;
        mRenderListMutex.unlock();
    }


    int packetSize = videoq.size();
    if(getRenderListSize() < maxRenderListSize && packetSize > 0){
        mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_DecodeToRender));
    }

    if(packetSize == 0){
        if(!mIsReadFinish && !mIsVideoLoadedCompleted){}
        else if(getRenderListSize() <= 1 && videoq.size() == 0){
            mIsVideoPlayed = true;
        }
    }
    if(mIsVideoPlayed && mIsAudioPlayed && mIsSubtitlePlayed && mIsReadFinish){
        //        qDebug() << "--------------- 播放完成A";
        emit statusChanged(AVDefine::MediaStatus_Played);
    }
    return time;
}

qint64 AVDecoder::nextTime(){
    int time = mIsReadFinish ? -1 : 0;
    if(hasVideo()){
        mRenderListMutex.lock();
        int minTime = time;
        for(int i = 0,len = mRenderList.size();i < len;i++){
            RenderItem *item = mRenderList[i];
            if(item->isRendered){
                continue;
            }
            item->mutex.lock();
            if(item->valid){
                if(minTime == time){
                    minTime = item->pts;
                }

                if(item->pts <= minTime){
                    minTime = item->pts;
                }
            }
            item->mutex.unlock();
        }
        time = minTime;
        mRenderListMutex.unlock();
    }
    return time;
}

void AVDecoder::initRenderList(){
    mRenderListMutex.lock();
    if(mRenderList.size() < maxRenderListSize){
        for(int i = mRenderList.size();i < maxRenderListSize;i++){
            mRenderList.push_back(new RenderItem);
        }
    }
    mRenderListMutex.unlock();
}

int AVDecoder::getRenderListSize(){
    int ret = 0;
    bool lock = mRenderListMutex.tryLock();
    for(int i = 0,len = mRenderList.size();i < len;i++){
        RenderItem *item = mRenderList[i];
        item->mutex.lock();
        if(item->valid)
            ++ret;
        item->mutex.unlock();
    }
    if(lock)
        mRenderListMutex.unlock();
    return ret;
}

void AVDecoder::clearRenderList(bool isDelete){
    bool lock = mRenderListMutex.tryLock();
    for(int i = 0,len = mRenderList.size();i < len;i++){
        RenderItem *item = mRenderList[i];
        item->mutex.lock();
        if(item->valid)
        {
            mVideoSwsCtxMutex.lock();
            mRenderList[i]->release(mVideoSwsCtx == NULL || isDelete);
            mVideoSwsCtxMutex.unlock();
        }
        item->mutex.unlock();
        if(isDelete){
            delete item;
        }
    }
    if(isDelete){
        mRenderList.clear();
    }
    if(lock)
        mRenderListMutex.unlock();
}

RenderItem *AVDecoder::getInvalidRenderItem(){
    RenderItem *item = NULL;
    bool lock = mRenderListMutex.tryLock();
    for(int i = 0,len = mRenderList.size();i < len;i++){
        RenderItem *item2 = mRenderList[i];
        item2->mutex.lock();
        if(!item2->valid){
            item = item2;
            item2->mutex.unlock();
            break;
        }
        item2->mutex.unlock();
    }
    if(lock)
        mRenderListMutex.unlock();
    return item;
}

void AVDecoder::changeRenderItemSize(int width,int height,AVPixelFormat format){
    bool lock = mRenderListMutex.tryLock();
    for(int i = 0,len = mRenderList.size();i < len;i++){
        RenderItem *item2 = mRenderList[i];
        item2->mutex.lock();
        if(item2->valid){
            mVideoSwsCtxMutex.lock();
            item2->release(mVideoSwsCtx == NULL);
            mVideoSwsCtxMutex.unlock();
        }

        int numBytes = av_image_get_buffer_size( (AVPixelFormat)format, width,height, 1  );
        uint8_t * buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
        av_image_fill_arrays( item2->frame->data, item2->frame->linesize, buffer, (AVPixelFormat)format,width,height, 1 );

        item2->mutex.unlock();
    }
    if(lock)
        mRenderListMutex.unlock();
}

void AVDecoder::statusChanged(AVDefine::MediaStatus status){
    if(mStatus == status)
        return;
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
        mCodec->checkRenderList();
        break;
    case AVCodecTaskCommand_SetFileName:{
        mCodec->setFilenameImpl(param2);
        break;
    }
    case AVCodecTaskCommand_DecodeToRender : {
        mCodec->checkRenderList();
        break;
    }
    case AVCodecTaskCommand_ShowFrameByPosition : {
        mCodec->showFrameByPositionImpl(param);
    }
    }
}
