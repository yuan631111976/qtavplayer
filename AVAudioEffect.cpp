#include "AVAudioEffect.h"
#include <QList>
#include <QMap>
#include <QtMath>
#include <QDebug>

AVAudioEffect::AVAudioEffect()
{

}


void AVAudioEffect::readLeftRightData(float *bytes,float *leftOutBytes,float *rightOutBytes,int len) {
    for (int i = 0; i < len; i++) {
        if(i % 2 == 0){
            leftOutBytes[i>>1] = bytes[i];
        }else{
            rightOutBytes[i>>1] = bytes[i];
        }
    }
}



void AVAudioEffect::removeVoice(float *bytes, float *outdata, int len, int channelCount) //去除人声
{
    //    qDebug() << "------channel  count : " << mSourceAudioFormat.channelCount();
    if(channelCount < 2){
        highPassFilter();
        return;
    }


    float *leftData = new float[len >> 1];
    float *rightData = new float[len >> 1];

    readLeftRightData(bytes,leftData,rightData,len);

    for (int i = 0; i < (len >> 1); i++) {
        outdata[i<<1] = leftData[i]-rightData[i]; //左声道减右声道
        outdata[(i<<1)+1] = rightData[i]-leftData[i];
    }
    delete [] leftData;
    delete [] rightData;
}

void AVAudioEffect::highPassFilter()//高通滤波
{
    //    https://github.com/zoeyangdw/Audio-Processing/blob/fd42e45a78b1e65e20a2810125502df05587d369/src/com/bupt/sdmda/yy/process/AudioProcesser.java
}

void AVAudioEffect::filter(float *bytes1,int len,float *bytes2,int len2,float *outbytes,int outlen,float *inbytes,int inlen){
    QList<float> qa;
    QList<float> qb;

    QMap<int, float> ha;
    QMap<int, float> hb;

    for(int i=1;i<len;i++){
        qb.append(0.0f);
        if(bytes1[i]!=0){
            hb.insert(i,bytes1[i]);
        }
    }
    for(int i=1;i<len2;i++){
        qa.append(0.0f);
        if(bytes2[i]!=0){
            ha.insert(i, bytes2[i]);
        }
    }

    for(int i = 0;i<inlen;i++){
        float cur = inbytes[i];
        float res = cur*bytes1[0];

        QMapIterator<int, float> iterator(hb);
        while (iterator.hasNext()) {
            iterator.next();
            res += qb.at(len - iterator.key() - 1) * iterator.value();
        }

        QMapIterator<int, float> iterator2(ha);
        while (iterator2.hasNext()) {
            iterator2.next();
            res -= qa.at(len2 - iterator2.key() - 1) * iterator2.value();
        }
        qb.append(cur);
        qb.pop_front();
        qa.append(res);
        qa.pop_front();

        outbytes[i]=res;
    }
}

void AVAudioEffect::hanning(int size,float *outbytes){
    for(int i = 0;i<size;i++){
        outbytes[i] = (float) (0.5 - 0.5*qCos(2*M_PI*i/size));
    }
}

void AVAudioEffect::ola(float rate, float *inbytes, int inlen, float *outbytes, int outlen, int sampleRate) {
    int wndSz = sampleRate/10;
    int anaStep = wndSz / 2;
    int synStep = (int) (anaStep*rate);
    int numWnd = (int) qCeil((inlen-wndSz)/anaStep+1);

    float *hanningWnd = new float[wndSz];
    hanning(wndSz,hanningWnd);


//    float ** frames = new float*[numWnd];
//    for (int i = 0; i < numWnd; i++) {
//        frames[i] = new float[wndSz];
//    }
    for (int i = 0; i < outlen; i++) {
        outbytes[i] = 0;
    }
    for (int i = 0; i < numWnd; i++) {
        for (int j = 0; j < wndSz; j++) {
            outbytes[i*synStep+j] = inbytes[i*anaStep+j]*hanningWnd[j];
//            frames[i][j] = inbytes[i*anaStep+j]*hanningWnd[j];
        }
    }
//    int N = wndSz+(numWnd-1)*synStep;
//    for (int i = 0; i < outlen; i++) {
//        outbytes[i] = 0;
//    }
//    for (int i = 0; i < numWnd; i++) {
//        for (int j = 0; j < wndSz; j++) {
//            outbytes[i*synStep+j] += frames[i][j];
//        }
//    }

    delete [] hanningWnd;
//    for (int i = 0; i < numWnd; i++) {
//        delete [] frames[i];
//    }
//    delete [] frames;
}

void AVAudioEffect::girl(float *inbytes,int inlen,float *outdata,int sampleRate){
    //ola(1.2f,inbytes,inlen,outdata,inlen,sampleRate);
    ola(0.8f,inbytes,inlen,outdata,inlen,sampleRate);
}

