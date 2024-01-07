#pragma once
extern "C" {
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
}
#include "stream_reader.h"
#include <fstream>
#include <iostream>
#include "tools.h"
class StreamReaderFfmpeg : public StreamReader {
    FfmpegGlobal *m_ffmpegGlobal;
    int m_state;
    StreamReaderObserver *m_observer;
    bool m_bRunning { false };
public:
    StreamReaderFfmpeg();
    virtual ~StreamReaderFfmpeg();

    int OpenStream(std::string strUrl, StreamReaderObserver *observer) override;
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

    std::thread *m_decodeThread;
    
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
    
    bool m_bEnableTestOut=true;
    
    int audio_frame_counter = 0;
    int video_frame_counter = 1;

    uint8_t *image_data_[4] = {nullptr};
    int image_data_line_size_[4] = {0};
    int image_dst_buffer_size = 0;
    
private:
    //for audio resample
    struct SwrContext *m_pSwrCtx;
    int max_dst_nb_samples = 0;
    int get_format_from_sample_fmt(const char **fmt,
                                                       enum AVSampleFormat sample_fmt);
    void fill_samples(double *dst, int nb_samples, int nb_channels, int sample_rate, double *t);
    int reSample(int64_t src_ch_layout,int src_rate,uint8_t **src_data,int src_nb_channels, AVSampleFormat src_sample_fmt,int src_nb_samples,
                  int64_t dst_ch_layout,int dst_rate,uint8_t ***dst_data_out,int dst_nb_channels, AVSampleFormat dst_sample_fmt);
};
