//
// Created by Faker on 2023/12/19.
//

#ifndef MUXER_PCM2AAC_H
#define MUXER_PCM2AAC_H


extern "C"
{
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class Pcm2AAC
{
public:
    Pcm2AAC();
    ~Pcm2AAC();

    bool Init(int i_nPcmSampleRate, AVSampleFormat i_ePcmSampleFmt, int i_nPcmChannels);
    void AddData(char *i_pData, int i_nSize);
    void FlushData(void);
    bool GetData(char *&o_pData, int &o_nSize);

private:
    void AddADTS(int i_nPktLen);

    int m_nPcmSize;
    int m_nPcmChannel;
    int m_nPcmSampleRate;
    AVSampleFormat m_ePcmFormat;
    char *m_aPcmPointer[AV_NUM_DATA_POINTERS];
    char *m_pPcmData;
    char *m_pOutData;
    AVPacket *m_pPacket;
    AVFrame *m_pFrame;
    const AVCodec *m_pCodec;
    AVCodecContext *m_pCodecCtx;
    SwrContext *m_pSwrCtx;
    int64_t m_s64Pts;
    int m_nFrameSize;
    int m_nFrameCnt;
};

#endif //MUXER_PCM2AAC_H
