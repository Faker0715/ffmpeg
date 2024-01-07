#pragma once
extern "C" {
#include "libavutil/avassert.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/bsf.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libswscale/swscale.h"
#include "libavutil/time.h"
#include "libavutil/imgutils.h"
#include "libavutil/pixdesc.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
#include "libavutil/timestamp.h"
}
#include "stream_writer.h"
#include "tools.h"
#include <fstream>
#include <iostream>
#include <mutex>

#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

// a wrapper around a single output AVStream
typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame *frame;
    AVFrame *tmp_frame;

    AVPacket *tmp_pkt;
    int frameIndex;
    int frameRate=30;//视频帧率
    int64_t  startTimeOffset;
    int64_t  timeStamp;
    int64_t  preTimeStamp;
    int bHaveIframe;
    int64_t  duration;
    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;

class StreamWriterFfmpeg : public StreamWriter {
    FfmpegGlobal *m_ffmpegGlobal;
    int m_state;
    StreamWriterObserver *m_observer;
    bool m_bRunning { false };
public:
    StreamWriterFfmpeg();
    virtual ~StreamWriterFfmpeg();

    int OpenStream(std::string strUrl, StreamWriterObserver *observer) override;
    int CloseStream() override;
    void SetNotDecoder(bool on) override;
    void OpenCodecContext(AVMediaType type, AVCodecContext **codec_context, int *stream_index) const;
    int DecodePacket(AVCodecContext *codec_context) ;
    int OutputAudioFrame(AVFrame *frame);
    int OutputVideoFrame(AVFrame *frame);
    void AllocImage();

private:
    AVFormatContext *m_pFormatCtx;
    AVCodecContext *m_pVideoCodecCtx;
    AVCodecContext *m_pAudioCodecCtx;
    AVFrame *m_pFrame;
    AVPacket m_packet;
    AVPacket m_packetOut;
    //AVPicture  m_pAVPicture;
    SwsContext * m_pSwsCtx;
    AVBSFContext *m_bsf_ctx = nullptr;
    AVBitStreamFilter *m_pfilter = nullptr;
    // Video
    int m_videoStreamIndex;
    int m_audioStreamIndex;
    int m_width=0;
    int m_height=0;
    int m_fps=0;

    std::thread *m_decodeThread = nullptr;
    
    bool m_bNotDecoder = true;

    int getSendOneFrameDelay(int64_t frameTime);
      uint64_t firstTime =0;
      uint64_t previousTime = 0;
    
private:
    //for test write file
    std::string output_video_;
    std::string output_audio_;

    std::fstream output_audio_stream_;
    std::fstream output_video_stream_;
    
    void initTestFile();
    
    bool m_bEnableTestOut=false;
    
    int audio_frame_counter = 0;
    int video_frame_counter = 1;

    uint8_t *image_data_[4] = {nullptr};
    int image_data_line_size_[4] = {0};
    int image_dst_buffer_size = 0;
    int m_dtsOffset=0;
private:
    //for audio resample
    struct SwrContext *m_pSwrCtx;
    int max_dst_nb_samples = 0;
    int get_format_from_sample_fmt(const char **fmt,
                                                       enum AVSampleFormat sample_fmt);
    void fill_samples(double *dst, int nb_samples, int nb_channels, int sample_rate, double *t);
    int reSample(int64_t src_ch_layout,int src_rate,uint8_t **src_data,int src_nb_channels, AVSampleFormat src_sample_fmt,int src_nb_samples,
                  int64_t dst_ch_layout,int dst_rate,uint8_t ***dst_data_out,int dst_nb_channels, AVSampleFormat dst_sample_fmt);


private:
  //for out stream
  OutputStream video_st, audio_st;
  const AVOutputFormat *fmt;
  const char *filename;
  AVFormatContext *oc;
  const AVCodec *audio_codec, *video_codec;
  int have_video = 0, have_audio = 0;
  int encode_video = 0, encode_audio = 0;
  AVDictionary *opt = NULL;

public:
  int openOutputStream(const char * url);
  int writeOutputStream();
  int freeOutputStream();
  int closeOutputStream();
  int sendRawVideoData(const uint8_t * data ,int datalen);
  int sendVideoData(const uint8_t * data ,int datalen);
  int sendRawAudioData(const uint8_t * data ,int datalen);
  int sendAudioData(const uint8_t * data ,int datalen);

  std::mutex m_mutex;
  bool m_bInited = false;
};
