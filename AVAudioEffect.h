#ifndef AVAUDIOEFFECT_H
#define AVAUDIOEFFECT_H

//声音特效

class AVAudioEffect
{
private:
    AVAudioEffect();
public :
    static void removeVoice(float *bytes, float *outdata, int len,int channelCount); //去除人声
    static void girl(float *inbytes,int inlen,float *outdata,int sampleRate);
private :
    static void highPassFilter();//高通滤波
    static void readLeftRightData(float *bytes,float *leftOutBytes,float *rightOutBytes,int len); // 获取左右通道
    static void filter(float *bytes1, int len, float *bytes2, int len2, float *outbytes, int outlen, float *inbytes, int inlen);
    static void ola(float rate, float *inbytes, int inlen, float *outbytes, int outlen, int sampleRate);
    static void hanning(int size, float *outbytes);
};

#endif // AVAUDIOEFFECT_H
