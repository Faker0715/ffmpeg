//
// Created by Faker on 2023/12/19.
//

#include "pcm2aac.h"

// refer to ffmpeg/libavformat/adtsenc.c
#define ADTS_HEADER_SIZE 7
#define ADTS_MAX_FRAME_BYTES ((1 << 13) - 1) // 8K

Pcm2AAC::Pcm2AAC()
{
    m_nPcmSize = 0;
    m_pPacket = NULL;
    m_pFrame = NULL;
    m_pCodec = NULL;
    m_pCodecCtx = NULL;
    m_pSwrCtx = NULL;
    m_s64Pts = 0;
    m_nFrameSize = 0;
    m_nFrameCnt = 0;

    for(int i = 0; i < AV_NUM_DATA_POINTERS; i++)
    {
        m_aPcmPointer[i] = new char[1024 * 10];
    }
    m_pPcmData = new char[1024 * 10];
    m_pOutData = new char[1024 * 10];
    memset(m_pPcmData, 0, 1024 * 10);
    memset(m_pOutData, 0, 1024 * 10);
}

Pcm2AAC::~Pcm2AAC()
{
    // just for test
    // 1秒钟处理了多少帧
    int nFramesPerSecond = m_nPcmSampleRate / m_nFrameSize; // 44100/1024=43
    printf("nFramesPerSecond = %d\n", nFramesPerSecond);
    // 1帧需要多少时间(毫秒)
    double fFrameTime = 1000.00 / nFramesPerSecond; // 1000/43=23.26ms
    printf("fFrameTime = %.2fms\n", fFrameTime);
    // 时间总长 = 总帧数 * 1帧所需时间
    float fDuration = m_nFrameCnt * fFrameTime / 1000; // 2628*23.25=61.12s
    printf("This AAC total frames = %d, duration = %.2fs\n", m_nFrameCnt, fDuration);

    if(m_pPacket != NULL)
    {
        av_packet_free(&m_pPacket);
    }
    if(m_pFrame != NULL)
    {
        av_frame_free(&m_pFrame);
    }
    if(m_pCodecCtx != NULL)
    {
        avcodec_free_context(&m_pCodecCtx);
    }
    if(m_pSwrCtx != NULL)
    {
        swr_free(&m_pSwrCtx);
    }

    for(int i = 0; i < AV_NUM_DATA_POINTERS; i++)
    {
        if(m_aPcmPointer[i] != NULL)
        {
            delete[] m_aPcmPointer[i];
        }
    }
    if(m_pPcmData != NULL)
    {
        delete[] m_pPcmData;
    }
    if(m_pOutData != NULL)
    {
        delete[] m_pOutData;
    }
}

bool Pcm2AAC::Init(int i_nPcmSampleRate, AVSampleFormat i_ePcmSampleFmt, int i_nPcmChannels)
{
    m_nPcmSampleRate = i_nPcmSampleRate;
    m_ePcmFormat = i_ePcmSampleFmt;
    m_nPcmChannel = i_nPcmChannels;

    // 寻找AAC的编码器
    m_pCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if(m_pCodec == NULL)
    {
        fprintf(stderr, "Codec not found\n");
        return false;
    }

    // 初始化编码器上下文, 主要是通道数, 采样率, 采样格式
    m_pCodecCtx = avcodec_alloc_context3(m_pCodec);
    if(m_pCodecCtx == NULL)
    {
        fprintf(stderr, "Could not allocate audio codec context\n");
        return false;
    }
    m_pCodecCtx->channels = m_nPcmChannel;
    m_pCodecCtx->channel_layout = av_get_default_channel_layout(m_nPcmChannel);
    m_pCodecCtx->sample_rate = m_nPcmSampleRate;
    // 新的FFmpeg, 只支持AV_SAMPLE_FMT_FLTP这一种AAC音频格式
    m_pCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    m_pCodecCtx->bit_rate = 64000;
    // Allow the use of the experimental AAC encoder
    m_pCodecCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    // 打开编码器
    if(avcodec_open2(m_pCodecCtx, m_pCodec, NULL) < 0)
    {
        fprintf(stderr, "Could not open codec\n");
        return false;
    }

    // 这个编码器的frame_size很关键, 表示每个声道每次只能编码frame_size个采样点, m_pFrame->nb_samples不能比它大
    // 否则会报错: more samples than frame size (avcodec_encode_audio2)
    printf("m_pCodecCtx->frame_size = %d\n", m_pCodecCtx->frame_size);
    m_nFrameSize = m_pCodecCtx->frame_size;

    // av_packet_alloc() 为一个NULL指针分配内存并将其值置为默认
    m_pPacket = av_packet_alloc();
    if(m_pPacket == NULL)
    {
        return false;
    }

    m_pFrame = av_frame_alloc();
    if(m_pFrame == NULL)
    {
        return false;
    }

    // 设置格式转换
    m_pSwrCtx = swr_alloc_set_opts(NULL, m_pCodecCtx->channel_layout, m_pCodecCtx->sample_fmt, m_pCodecCtx->sample_rate,
                                   m_pCodecCtx->channel_layout, m_ePcmFormat, m_nPcmSampleRate, 0, NULL);
    if(m_pSwrCtx == NULL)
    {
        fprintf(stderr, "Could not allocate resample context\n");
        return false;
    }

    if(swr_init(m_pSwrCtx) < 0)
    {
        fprintf(stderr, "Could not open resample context\n");
        return false;
    }

    return true;
}

void Pcm2AAC::AddData(char *i_pData, int i_nSize)
{
    int ret = 0;
    int nSampleSize = 0;

    memcpy(m_pPcmData + m_nPcmSize, i_pData, i_nSize);
    m_nPcmSize += i_nSize;
    nSampleSize = av_get_bytes_per_sample(m_ePcmFormat);
    if(m_nPcmSize <= (nSampleSize * m_nFrameSize * m_nPcmChannel))
    {
        return;
    }
    memcpy(m_aPcmPointer[0], m_pPcmData, nSampleSize * m_nFrameSize * m_nPcmChannel);

    m_nPcmSize -= (nSampleSize * m_nFrameSize * m_nPcmChannel);
    memcpy(m_pPcmData, m_pPcmData + nSampleSize * m_nFrameSize * m_nPcmChannel, m_nPcmSize);

    m_pFrame->pts = m_s64Pts;
    m_s64Pts += m_nFrameSize; // fixme
    m_pFrame->nb_samples = m_nFrameSize;
    m_pFrame->format = m_pCodecCtx->sample_fmt;
    m_pFrame->channel_layout = m_pCodecCtx->channel_layout;
    m_pFrame->sample_rate = m_pCodecCtx->sample_rate;
    if(av_frame_get_buffer(m_pFrame, 0) < 0)
    {
        fprintf(stderr, "Could not allocate audio data buffers\n");
        return ;
    }

    // 转换格式
    if(swr_convert(m_pSwrCtx, m_pFrame->extended_data, m_pFrame->nb_samples, (const uint8_t**)m_aPcmPointer, m_pFrame->nb_samples) < 0)
    {
        fprintf(stderr, "Could not convert input samples (error )\n");
        if(m_pFrame != NULL)
        {
            av_frame_unref(m_pFrame);
        }
        return ;
    }

    // 放进编码器
    ret = avcodec_send_frame(m_pCodecCtx, m_pFrame);
    if(ret < 0)
    {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        if(m_pFrame != NULL)
        {
            av_frame_unref(m_pFrame);
        }
        return;
    }
    if(m_pFrame != NULL)
    {
        av_frame_unref(m_pFrame);
    }
}

void Pcm2AAC::FlushData(void)
{
    // 发送NULL表示刷新packet, 意味着流的结束
    avcodec_send_frame(m_pCodecCtx, NULL);
}

bool Pcm2AAC::GetData(char *&o_pData, int &o_nSize)
{
    // 从编码器取出编码完的数据
    int ret = avcodec_receive_packet(m_pCodecCtx, m_pPacket);
    if(ret < 0)
    {
        return false;
    }

    // 由于编码后的AAC流没有ADTS头, 会导致保存的文件无法被播放器识别, 所以需要手动写入ADTS头
    // 在每个AAC原始流前面加上一个7字节的ADTS头
    AddADTS(m_pPacket->size + ADTS_HEADER_SIZE);
    m_nFrameCnt++;

    memcpy(m_pOutData + ADTS_HEADER_SIZE, m_pPacket->data, m_pPacket->size);
    o_nSize = m_pPacket->size + ADTS_HEADER_SIZE;
    o_pData = m_pOutData;
    av_packet_unref(m_pPacket);
    return true;
}

// ADTS全称是(Audio Data Transport Stream), 是AAC的一种十分常见的传输格式
void Pcm2AAC::AddADTS(int i_nPktLen)
{
    int nProfile = 1;   // AAC LC, 低复杂度规格
    int nFreqIdx = 4;   // default 44.1kHz
    int nChanCfg = m_nPcmChannel;

    if(i_nPktLen > ADTS_MAX_FRAME_BYTES)
    {
        fprintf(stderr, "ADTS frame size too large: %d (max %d)\n", i_nPktLen, ADTS_MAX_FRAME_BYTES);
        return;
    }

    switch(m_nPcmSampleRate)
    {
        case 96000:
            nFreqIdx = 0;
            break;
        case 88200:
            nFreqIdx = 1;
            break;
        case 64000:
            nFreqIdx = 2;
            break;
        case 48000:
            nFreqIdx = 3;
            break;
        case 44100:
            nFreqIdx = 4;
            break;
        case 32000:
            nFreqIdx = 5;
            break;
        case 24000:
            nFreqIdx = 6;
            break;
        case 22050:
            nFreqIdx = 7;
            break;
        case 16000:
            nFreqIdx = 8;
            break;
        case 12000:
            nFreqIdx = 9;
            break;
        case 11025:
            nFreqIdx = 10;
            break;
        case 8000:
            nFreqIdx = 11;
            break;
        case 7350:
            nFreqIdx = 12;
            break;
        default:
            break;
    }

    // Fill in ADTS header, 7 bytes
    m_pOutData[0] = 0xFF;
    m_pOutData[1] = 0xF1;
    m_pOutData[2] = ((nProfile) << 6) + (nFreqIdx << 2) + (nChanCfg >> 2);
    m_pOutData[3] = (((nChanCfg & 3) << 6) + (i_nPktLen >> 11));
    m_pOutData[4] = ((i_nPktLen & 0x7FF) >> 3);
    m_pOutData[5] = (((i_nPktLen & 7) << 5) + 0x1F);
    m_pOutData[6] = 0xFC;
}

