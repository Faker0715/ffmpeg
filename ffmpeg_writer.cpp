#include "stream_writer.h"
#include "ffmpeg_writer.h"
#include "tools.h"
#include<sys/time.h>
//输入裸流解决方案
//https://itecnote.com/tecnote/c-compute-pts-and-dts-correctly-to-sync-audio-and-video-ffmpeg-c/
static int ffmpeg_interrupt_cb(void* p)
{
    //printf("#### ERROR ####\n");
    return 0;
}
static inline int64_t currentTimeMs()
{
    timeval time;
    gettimeofday(&time, nullptr);
    return ((time.tv_sec * 1000) + (time.tv_usec / 1000));
}
StreamWriterFfmpeg::StreamWriterFfmpeg() {
    m_ffmpegGlobal = SingletonFF<FfmpegGlobal>::instance();
    m_pVideoCodecCtx = NULL;
    m_pAudioCodecCtx = NULL;
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_pFormatCtx = avformat_alloc_context();
    m_pSwsCtx = nullptr;
    m_pSwrCtx = nullptr;

    AVIOInterruptCB cb;
    cb.callback = ffmpeg_interrupt_cb;
    cb.opaque = this;
    m_pFormatCtx->interrupt_callback = cb;

    m_pFrame = av_frame_alloc();
    m_pfilter = (AVBitStreamFilter *)av_bsf_get_by_name("h264_mp4toannexb");
    av_bsf_alloc(m_pfilter, &m_bsf_ctx);

    memset(&video_st,0,sizeof(OutputStream));
    if(video_st.startTimeOffset == 0) {
        video_st.startTimeOffset = currentTimeMs();
    }
    memset(&audio_st,0,sizeof(OutputStream));
    if(audio_st.startTimeOffset == 0) {
        audio_st.startTimeOffset = currentTimeMs();
    }
}

StreamWriterFfmpeg::~StreamWriterFfmpeg() {
    avformat_close_input(&m_pFormatCtx);
    avformat_free_context(m_pFormatCtx);
    av_frame_free(&m_pFrame);
    if (m_pSwsCtx) sws_freeContext(m_pSwsCtx);
    avcodec_free_context(&this->m_pVideoCodecCtx);
    avcodec_free_context(&this->m_pAudioCodecCtx);
    av_bsf_free(&m_bsf_ctx);
    if(m_pSwrCtx) {
        swr_free(&m_pSwrCtx);
    }
   
    this->output_video_stream_.close();
    this->output_audio_stream_.close();
}
void StreamWriterFfmpeg::initTestFile()
{
    if(!m_bEnableTestOut) {
        return;
    }
    this->output_audio_ = "/Users/yanhua/Downloads/out.pcm";
    this->output_video_ = "/Users/yanhua/Downloads/out.yuv";
    this->output_audio_stream_.open(this->output_audio_.c_str(), std::ios::binary | std::ios::out);
    this->output_video_stream_.open(this->output_video_.c_str(), std::ios::binary | std::ios::out);

    if (this->output_video_stream_.bad()) {
        throw std::runtime_error("Open output video file failed.");
    }
    if (this->output_audio_stream_.bad()) {
        throw std::runtime_error("Open output audio file failed.");
    }

    /*
     
     播放音频pcm文件
     ffplay -f f32le -ac 1 -ar 44100 ./out.pcm
     ffplay -f s16le -ac 1 -ar 48000 ./out.pcm

     播放视频yuv文件
     ffplay -f rawvideo -pix_fmt yuv420p -video_size 1280x720 ./out.yuv
     ————————————————
     版权声明：本文为CSDN博主「blueberry_mu」的原创文章，遵循CC 4.0 BY-SA版权协议，转载请附上原文出处链接及本声明。
     原文链接：https://blog.csdn.net/a992036795/article/details/123059880
     */
}


/**
 *
 * @param type 音频/视频
 */
void StreamWriterFfmpeg::OpenCodecContext(AVMediaType type, AVCodecContext **codec_context, int *stream_index) const {
    // find video
    *stream_index = av_find_best_stream(m_pFormatCtx,
                                        type, -1,
                                        -1, nullptr, 0);

    if (*stream_index < 0) {
        // stream not found
        if(type == AVMEDIA_TYPE_VIDEO) {
            throw std::runtime_error("stream not found");
        } else {
            return;
        }
       
    }
    

    AVStream *stream = m_pFormatCtx->streams[*stream_index];

    if (stream == nullptr) {
        throw std::runtime_error("Find video stream failure.");
    }

    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == nullptr) {
        throw std::runtime_error("Find video codec failure.");
    }
    *codec_context = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(*codec_context, stream->codecpar) < 0) {
        throw std::runtime_error("Fill parameters failure.");
    }
    if (avcodec_open2(*codec_context, codec, nullptr) < 0) {
        throw std::runtime_error("Open avcodec failure.");
    }

}
//https://itecnote.com/tecnote/c-compute-pts-and-dts-correctly-to-sync-audio-and-video-ffmpeg-c/
int StreamWriterFfmpeg::DecodePacket(AVCodecContext *codec_context) {
    int ret = 0;

    ret = avcodec_send_packet(codec_context, &this->m_packet);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return 0;
    }
    if (ret == AVERROR(EINVAL)) {
        //throw std::runtime_error(vformat("avcodec_send_packet failure. error:%s", av_err2str(ret)));
        //return ret;
    }
    if (ret == AVERROR_INPUT_CHANGED) {
       // throw std::runtime_error(vformat("avcodec_send_packet failure. error:%s", av_err2str(ret)));
        //return ret;
    }
    if (ret < 0) {
       // throw std::runtime_error(vformat("avcodec_send_packet failure. error:%s", av_err2str(ret)));
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(codec_context, this->m_pFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }
        if (ret == AVERROR(EINVAL)) {
           // throw std::runtime_error(vformat("avcodec_receive_failure. error:%s", av_err2str(ret)));
            //return ret;
        }
        if (ret == AVERROR_INPUT_CHANGED) {
           // throw std::runtime_error(vformat("avcodec_receive_failure failure. error:%s", av_err2str(ret)));
            //return ret;
        }
        if (ret < 0) {
            return ret;
        }
        if (codec_context->codec_type == AVMEDIA_TYPE_AUDIO) {
            ret = OutputAudioFrame(this->m_pFrame);
        } else if (codec_context->codec_type == AVMEDIA_TYPE_VIDEO) {
            ret = OutputVideoFrame(this->m_pFrame);
        }
        av_frame_unref(this->m_pFrame);
        // if dump frame failed , return ret;
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}
int StreamWriterFfmpeg::OutputAudioFrame(AVFrame *frame) {
    
    int unpadded_line_size = frame->nb_samples * av_get_bytes_per_sample(AVSampleFormat(frame->format));
    
    int64_t src_ch_layout= av_get_default_channel_layout(m_pAudioCodecCtx->channels);
    int src_rate = m_pAudioCodecCtx->sample_rate;
    uint8_t **src_data = &frame->extended_data[0];
    int src_nb_channels= m_pAudioCodecCtx->channels;
    AVSampleFormat src_sample_fmt = m_pAudioCodecCtx->sample_fmt;
    int src_nb_samples = frame->nb_samples;//unpadded_line_size;
    int64_t dst_ch_layout = av_get_default_channel_layout(1);
    int dst_rate = 48000;
    uint8_t **dst_data = NULL;
    int dst_nb_channels = 1;
    AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_S16;
    int frame_size = reSample( src_ch_layout, src_rate,src_data, src_nb_channels,  src_sample_fmt, src_nb_samples,
                               dst_ch_layout, dst_rate,&dst_data,dst_nb_channels,  dst_sample_fmt);
    
    
    if(!m_bEnableTestOut) {
        return 0;
    }
    if(dst_data == NULL) {
        return 0;
    }
    //std::cout << vformat("Write audio pre frame %d,size=%d", this->audio_frame_counter++, unpadded_line_size) << std::endl;
    //std::cout << vformat("Write audio sample frame %d,size=%d", this->audio_frame_counter++, frame_size) << std::endl;
    //output_audio_stream_.write(reinterpret_cast<const char *>(frame->extended_data[0]), unpadded_line_size);
    output_audio_stream_.write(reinterpret_cast<const char *>(dst_data[0]), frame_size);
    
    if (dst_data)
        av_freep(&dst_data[0]);
}


int StreamWriterFfmpeg::OutputVideoFrame(AVFrame *frame) {
    
    if(!m_bEnableTestOut) {
        return 0;
    }
    
    if (frame->width != this->m_pVideoCodecCtx->width
        || frame->height != this->m_pVideoCodecCtx->height
        || frame->format != this->m_pVideoCodecCtx->pix_fmt) {
        throw std::runtime_error("The video frame width,height and fmt must same .");
    }
    av_image_copy(image_data_,
                  image_data_line_size_,
                  const_cast<const uint8_t ** > ( reinterpret_cast< uint8_t **>(frame->data)),
                  frame->linesize,
                  this->m_pVideoCodecCtx->pix_fmt,
                  frame->width,
                  frame->height
    );
    // std::cout << vformat("Write video frame %d,size=%d,width=%d,height=%d,fmt=%s",
    //                      this->video_frame_counter++,
    //                      this->image_dst_buffer_size,
    //                      frame->width,
    //                      frame->height,
    //                      av_get_pix_fmt_name(AVPixelFormat(frame->format)))
    //           << std::endl;
    output_video_stream_.write(reinterpret_cast<const char *>(this->image_data_[0]), this->image_dst_buffer_size);
}

int StreamWriterFfmpeg::OpenStream(std::string strUrl, StreamWriterObserver *observer)
{
    observer->setWriter(this);
    initTestFile();
    
   
    
    
    m_observer = observer;
   
    openOutputStream(strUrl.c_str());

    return 0;

}
void StreamWriterFfmpeg::AllocImage() {

    this->image_dst_buffer_size = av_image_alloc(this->image_data_,
                                                 this->image_data_line_size_,
                                                 this->m_pVideoCodecCtx->width,
                                                 this->m_pVideoCodecCtx->height,
                                                 this->m_pVideoCodecCtx->pix_fmt,
                                                 1);
    if (this->image_dst_buffer_size < 0) {
        //throw std::runtime_error(vformat("Alloc image error,message:%s", av_err2str(this->image_dst_buffer_size)));
    }
}


int StreamWriterFfmpeg::CloseStream()
{
    return 0;
    m_state = StreamStateStopped;
    closeOutputStream();
    freeOutputStream();
    if(m_decodeThread != nullptr) {
        m_decodeThread->join();
        delete m_decodeThread;
    }
    if(m_bsf_ctx != nullptr) {
        //av_bsf_free(&m_bsf_ctx);

    }
    return 0;
}
void StreamWriterFfmpeg::SetNotDecoder(bool on)
{
    m_bNotDecoder = on;
}
int StreamWriterFfmpeg::getSendOneFrameDelay(int64_t frameTime)

{

    uint64_t sleep_time=0;

    if (this->firstTime==0) {

    this->firstTime = 1;

    this->previousTime = std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1000 / 1000;   // targetTime is the time we want to delay to

    //ngn_log(NGN_LOG_DEBUG,"mod_send_one_frame_delay start");

    return 1;

    }

    // Set the new target

    this->previousTime += frameTime;

    // Calculate the sleep time so we delay until the target time

    //mtime_t now = mdate();

    sleep_time = this->previousTime-(std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1000 / 1000) ;

    if (sleep_time > 0)

    {

    //ngn_log(NGN_LOG_INFO,"sleep_time=%d",sleep_time);

    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

    }

    return sleep_time <= -frameTime;

}
int StreamWriterFfmpeg::get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;

    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }

    fprintf(stderr,
            "Sample format %s not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return AVERROR(EINVAL);
}

/**
 * Fill dst buffer with nb_samples, generated starting from t.
 */
void StreamWriterFfmpeg::fill_samples(double *dst, int nb_samples, int nb_channels, int sample_rate, double *t)
{
    int i, j;
    double tincr = 1.0 / sample_rate, *dstp = dst;
    const double c = 2 * M_PI * 440.0;

    /* generate sin tone with 440Hz frequency and duplicated channels */
    for (i = 0; i < nb_samples; i++) {
        *dstp = sin(c * *t);
        for (j = 1; j < nb_channels; j++)
            dstp[j] = dstp[0];
        dstp += nb_channels;
        *t += tincr;
    }
}
int StreamWriterFfmpeg::reSample(int64_t src_ch_layout,int src_rate,uint8_t **src_data,int src_nb_channels, AVSampleFormat src_sample_fmt,int src_nb_samples,
              int64_t dst_ch_layout,int dst_rate,uint8_t ***dst_data_out,int dst_nb_channels, AVSampleFormat dst_sample_fmt)
{
    int src_linesize, dst_linesize;
    int dst_nb_samples;
    int dst_bufsize;
    uint8_t **dst_data = NULL;
    const char *fmt;
    struct SwrContext *swr_ctx;
    double t;
    int ret;
    if(m_pSwrCtx == nullptr) {
        /* create resampler context */
        m_pSwrCtx = swr_ctx = swr_alloc();
        if (!swr_ctx) {
            fprintf(stderr, "Could not allocate resampler context\n");
            ret = AVERROR(ENOMEM);
            goto end;
        }
        /* set options */
        av_opt_set_int(swr_ctx, "in_channel_layout",    src_ch_layout, 0);
        av_opt_set_int(swr_ctx, "in_sample_rate",       src_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);

        av_opt_set_int(swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
        av_opt_set_int(swr_ctx, "out_sample_rate",       dst_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

        /* initialize the resampling context */
        if ((ret = swr_init(swr_ctx)) < 0) {
            fprintf(stderr, "Failed to initialize the resampling context\n");
            goto end;
        }
        /* allocate source and destination samples buffers */

//        src_nb_channels = av_get_channel_layout_nb_channels(src_ch_layout);
//        ret = av_samples_alloc_array_and_samples(&src_data, &src_linesize, src_nb_channels,
//                                                     src_nb_samples, src_sample_fmt, 0);
//        if (ret < 0) {
//            fprintf(stderr, "Could not allocate source samples\n");
//            goto end;
//        }

    } else {
        swr_ctx = m_pSwrCtx;
    }


        t = 0;
        if(true) {
         
          

            /* compute the number of converted samples: buffering is avoided
                * ensuring that the output buffer will contain at least all the
                * converted input samples */
            max_dst_nb_samples = dst_nb_samples =
                av_rescale_rnd(src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);

            /* buffer is going to be directly written to a rawaudio file, no alignment */
            dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
            ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
                                                         dst_nb_samples, dst_sample_fmt, 0);
            if (ret < 0) {
                fprintf(stderr, "Could not allocate destination samples\n");
                goto end;
            }
            
            /* generate synthetic audio */
           // fill_samples((double *)src_data[0], src_nb_samples, src_nb_channels, src_rate, &t);

            /* compute destination number of samples */
            dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, src_rate) +
                                            src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
            if (dst_nb_samples > max_dst_nb_samples) {
                av_freep(&dst_data[0]);
                ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,
                                       dst_nb_samples, dst_sample_fmt, 1);
                if (ret < 0)
                    goto end;
                max_dst_nb_samples = dst_nb_samples;
            }
           
            /* convert to destination format */
            ret = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)src_data, src_nb_samples);
            if (ret < 0) {
                fprintf(stderr, "Error while converting\n");
                goto end;
            }
            dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
                                                     ret, dst_sample_fmt, 1);
            if (dst_bufsize < 0) {
                fprintf(stderr, "Could not get sample buffer size\n");
                goto end;
            }
            printf("t:%f in:%d out:%d\n", t, src_nb_samples, ret);
            //fwrite(dst_data[0], 1, dst_bufsize, dst_file);
            //output_audio_stream_.write(reinterpret_cast<const char *>(dst_data[0]), dst_bufsize);
            *dst_data_out = dst_data;
            return dst_bufsize;
        } ;

        if ((ret = get_format_from_sample_fmt(&fmt, dst_sample_fmt)) < 0)
            goto end;
//        fprintf(stderr, "Resampling succeeded. Play the output file with the command:\n"
//                "ffplay -f %s -channel_layout %"PRId64" -channels %d -ar %d %s\n",
//                fmt, dst_ch_layout, dst_nb_channels, dst_rate, dst_filename);

end:
    
    return 0;
}
static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
  //  AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    // printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
    //        av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
    //        av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
    //        av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
    //        pkt->stream_index);
}
static bool isSpsStartHead(uint8_t * data,int datalen)
{
    if(datalen <=5) {
        return false;
    }
    if ((data[4] & 0x1f) == 7 ) {
        return true;
    }
    return false;
}
static int write_h264_video_frame(OutputStream *ost,AVFormatContext *fmt_ctx, AVCodecContext *c,
                       AVStream *st, AVPacket *pkt,int *frameIndex)
{
    int ret;
    ost->timeStamp = currentTimeMs()-ost->startTimeOffset;
    int64_t dts = ost->timeStamp / (av_q2d(st->time_base) * 1000);//av_gettime() / 1000; // convert into millisecond
    pkt->pts =dts;
    pkt->dts =dts;
    ost->duration = ost->timeStamp - ost->preTimeStamp;
    ost->preTimeStamp = ost->timeStamp;
    if (ost->duration <= 0) {
        ost->duration = 1;
    }

    //pkt->flags = 0;
    if(isSpsStartHead(pkt->data,pkt->size)) {
        pkt->flags |= AV_PKT_FLAG_KEY;
        ost->bHaveIframe = 1;
    }

    if(ost->bHaveIframe == 0) {
        return 0;
    }

   // (*frameIndex)++;
    /* rescale output packet timestamp values from codec to stream timebase */
   // av_packet_rescale_ts(pkt, c->time_base, st->time_base);
    pkt->stream_index = st->index;

    /* Write the compressed frame to the media file. */
    log_packet(fmt_ctx, pkt);
    ret = av_interleaved_write_frame(fmt_ctx, pkt);
    /* pkt is now blank (av_interleaved_write_frame() takes ownership of
     * its contents and resets pkt), so that no unreferencing is necessary.
     * This would be different if one used av_write_frame(). */
    if (ret < 0) {
        //fprintf(stderr, "Error while writing output packet: %s\n", av_err2str(ret));
        return 0;
    }
    //av_packet_free(&pkt);
    return ret == AVERROR_EOF ? 1 : 0;
}
static int write_raw_audio_frame(AVFormatContext *fmt_ctx, AVCodecContext *c,
                                  AVStream *st, AVPacket *pkt,int *frameIndex)
{
    int ret;
    int64_t dts = av_gettime() / 1000; // convert into millisecond
    dts = dts * STREAM_FRAME_RATE;
    int m_dtsOffset=0;
    if(m_dtsOffset < 0) {
        m_dtsOffset = dts;
    }

    pkt->pts =AV_NOPTS_VALUE;
    pkt->dts =(dts - m_dtsOffset);
    //pkt->flags = 0;
    pkt->flags |= AV_PKT_FLAG_KEY;

    /* rescale output packet timestamp values from codec to stream timebase */
    //av_packet_rescale_ts(pkt, c->time_base, st->time_base);
    pkt->stream_index = st->index;

    /* Write the compressed frame to the media file. */
    log_packet(fmt_ctx, pkt);
    ret = av_interleaved_write_frame(fmt_ctx, pkt);
    /* pkt is now blank (av_interleaved_write_frame() takes ownership of
     * its contents and resets pkt), so that no unreferencing is necessary.
     * This would be different if one used av_write_frame(). */
    if (ret < 0) {
        //fprintf(stderr, "Error while writing output packet: %s\n", av_err2str(ret));
        return 0;
    }
    //av_packet_free(&pkt);
    return ret == AVERROR_EOF ? 1 : 0;
}
static int write_frame(AVFormatContext *fmt_ctx, AVCodecContext *c,
                       AVStream *st, AVFrame *frame, AVPacket *pkt)
{
    int ret;

    // send the frame to the encoder
    ret = avcodec_send_frame(c, frame);
    if (ret < 0) {
        // fprintf(stderr, "Error sending a frame to the encoder: %s\n",
        //         av_err2str(ret));
        return 0;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(c, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            //fprintf(stderr, "Error encoding a frame: %s\n", av_err2str(ret));
            return 0;
        }

        /* rescale output packet timestamp values from codec to stream timebase */
        av_packet_rescale_ts(pkt, c->time_base, st->time_base);
        pkt->stream_index = st->index;

        /* Write the compressed frame to the media file. */
        log_packet(fmt_ctx, pkt);
        ret = av_interleaved_write_frame(fmt_ctx, pkt);
        /* pkt is now blank (av_interleaved_write_frame() takes ownership of
         * its contents and resets pkt), so that no unreferencing is necessary.
         * This would be different if one used av_write_frame(). */
        if (ret < 0) {
            //fprintf(stderr, "Error while writing output packet: %s\n", av_err2str(ret));
            return 0;
        }
    }

    return ret == AVERROR_EOF ? 1 : 0;
}

/* Add an output stream. */
static void add_stream(OutputStream *ost, AVFormatContext *oc,
                       const AVCodec **codec,
                       enum AVCodecID codec_id)
{
    AVCodecContext *c;
    int i;

    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        return;
    }

    ost->tmp_pkt = av_packet_alloc();
    if (!ost->tmp_pkt) {
        fprintf(stderr, "Could not allocate AVPacket\n");
        return;
    }

    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        fprintf(stderr, "Could not allocate stream\n");
        return;
    }
    ost->st->id = oc->nb_streams-1;
    c = avcodec_alloc_context3(*codec);
    if (!c) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        return;
    }
    ost->enc = c;

    switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt  = (*codec)->sample_fmts ?
            (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        c->bit_rate    = 64000;
        c->sample_rate = 44100;
        if ((*codec)->supported_samplerates) {
            c->sample_rate = (*codec)->supported_samplerates[0];
            for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                if ((*codec)->supported_samplerates[i] == 44100)
                    c->sample_rate = 44100;
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        c->channel_layout = AV_CH_LAYOUT_STEREO;
        if ((*codec)->channel_layouts) {
            c->channel_layout = (*codec)->channel_layouts[0];
            for (i = 0; (*codec)->channel_layouts[i]; i++) {
                if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                    c->channel_layout = AV_CH_LAYOUT_STEREO;
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        ost->st->time_base = (AVRational){ 1, c->sample_rate };
        break;

    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;

        c->bit_rate = 400000;
        /* Resolution must be a multiple of two. */
        c->width    = 1920;
        c->height   = 1080;
        /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
        //如果是yuv编码用下面的方式计算时间基
        //ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
        //如果是写裸流的话用采样hz替代
        ost->st->time_base = (AVRational){ 1, 90000 };
        c->time_base       =  (AVRational){ 1, STREAM_FRAME_RATE };//ost->st->time_base;

        c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
        c->pix_fmt       = STREAM_PIX_FMT;
        if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
            /* just for testing, we also add B-frames */
            c->max_b_frames = 2;
        }
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
            /* Needed to avoid using macroblocks in which some coeffs overflow.
             * This does not happen with normal video, it just happens here as
             * the motion of the chroma plane does not match the luma plane. */
            c->mb_decision = 2;
        }
        break;

    default:
        break;
    }

    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}


/**************************************************************/
/* audio output */

static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    int ret;

    if (!frame) {
        fprintf(stderr, "Error allocating an audio frame\n");
        return NULL;
    }

    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples) {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            fprintf(stderr, "Error allocating an audio buffer\n");
            return NULL;
        }
    }

    return frame;
}

static void open_audio(AVFormatContext *oc, const AVCodec *codec,
                       OutputStream *ost, AVDictionary *opt_arg)
{
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;

    c = ost->enc;

    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        //fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
        return;
    }

    /* init signal generator */
    ost->t     = 0;
    ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
    /* increment frequency by 110 Hz per second */
    ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000;
    else
        nb_samples = c->frame_size;

    ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
                                       c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
                                       c->sample_rate, nb_samples);

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        return;
    }

    /* create resampler context */
    ost->swr_ctx = swr_alloc();
    if (!ost->swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        return;
    }

    /* set options */
    av_opt_set_int       (ost->swr_ctx, "in_channel_count",   c->channels,       0);
    av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
    av_opt_set_int       (ost->swr_ctx, "out_channel_count",  c->channels,       0);
    av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);

    /* initialize the resampling context */
    if ((ret = swr_init(ost->swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        return;
    }
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */
static AVFrame *get_audio_frame(OutputStream *ost,const uint8_t * data,int datalen)
{
    AVFrame *frame = ost->tmp_frame;
    int j, i, v;
    int16_t *q = (int16_t*)frame->data[0];

    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, ost->enc->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) > 0)
        return NULL;

    if(data != NULL) {
      memcpy(q,data,datalen);
    } else {
      for (j = 0; j <frame->nb_samples; j++) {
          v = (int)(sin(ost->t) * 10000);
          for (i = 0; i < ost->enc->channels; i++)
              *q++ = v;
          ost->t     += ost->tincr;
          ost->tincr += ost->tincr2;
      }
    }



    frame->pts = ost->next_pts;
    ost->next_pts  += frame->nb_samples;

    return frame;
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_audio_frame(AVFormatContext *oc, OutputStream *ost,const uint8_t * data ,int datalen)
{
    AVCodecContext *c;
    AVFrame *frame;
    int ret;
    int dst_nb_samples;

    c = ost->enc;

    frame = get_audio_frame(ost,data,datalen);

    if (frame) {
        /* convert samples from native format to destination codec format, using the resampler */
        /* compute destination number of samples */
        dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
                                        c->sample_rate, c->sample_rate, AV_ROUND_UP);
        //av_assert0(dst_nb_samples == frame->nb_samples);

        /* when we pass a frame to the encoder, it may keep a reference to it
         * internally;
         * make sure we do not overwrite it here
         */
        ret = av_frame_make_writable(ost->frame);
        if (ret < 0)
            return 0;

        /* convert to destination format */
        ret = swr_convert(ost->swr_ctx,
                          ost->frame->data, dst_nb_samples,
                          (const uint8_t **)frame->data, frame->nb_samples);
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            return 0;
        }
        frame = ost->frame;

        frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
        ost->samples_count += dst_nb_samples;
    }

    return write_frame(oc, c, ost->st, frame, ost->tmp_pkt);
}

/**************************************************************/
/* video output */

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame *picture;
    int ret;

    picture = av_frame_alloc();
    if (!picture)
        return NULL;

    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate frame data.\n");
        return NULL;
    }

    return picture;
}

static void open_video(AVFormatContext *oc, const AVCodec *codec,
                       OutputStream *ost, AVDictionary *opt_arg)
{
    int ret;
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;

    av_dict_copy(&opt, opt_arg, 0);

    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        return;
    }

    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ost->frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        return;
    }

    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            fprintf(stderr, "Could not allocate temporary picture\n");
            return;
        }
    }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        return;
    }
}

/* Prepare a dummy image. */
static void fill_yuv_image(AVFrame *pict, int frame_index,
                           int width, int height,const uint8_t * data,int datalen)
{
    int x, y, i;

    i = frame_index;

    if(data != NULL) {
      memcpy(pict->data[0],data,datalen);
      return;
    }

    /* Y */
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;

    /* Cb and Cr */
    for (y = 0; y < height / 2; y++) {
        for (x = 0; x < width / 2; x++) {
            pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
            pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
        }
    }
}

static AVFrame *get_video_frame(OutputStream *ost,const uint8_t * data,int datalen)
{
    AVCodecContext *c = ost->enc;

    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, c->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) > 0)
        return NULL;

    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if (av_frame_make_writable(ost->frame) < 0)
        return NULL;

    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        /* as we only generate a YUV420P picture, we must convert it
         * to the codec pixel format if needed */
        if (!ost->sws_ctx) {
            ost->sws_ctx = sws_getContext(c->width, c->height,
                                          AV_PIX_FMT_YUV420P,
                                          c->width, c->height,
                                          c->pix_fmt,
                                          SCALE_FLAGS, NULL, NULL, NULL);
            if (!ost->sws_ctx) {
                fprintf(stderr,
                        "Could not initialize the conversion context\n");
                return NULL;
            }
        }
       fill_yuv_image(ost->tmp_frame, ost->next_pts, c->width, c->height,data,datalen);
       
        sws_scale(ost->sws_ctx, (const uint8_t * const *) ost->tmp_frame->data,
                  ost->tmp_frame->linesize, 0, c->height, ost->frame->data,
                  ost->frame->linesize);
    } else {
        fill_yuv_image(ost->frame, ost->next_pts, c->width, c->height,data,datalen);
    }

    ost->frame->pts = ost->next_pts++;

    return ost->frame;
}

/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_video_frame(AVFormatContext *oc, OutputStream *ost,const uint8_t * data ,int datalen)
{
    //return write_frame(oc, ost->enc, ost->st, get_video_frame(ost,data,datalen), ost->tmp_pkt);
    ost->tmp_pkt->data=(uint8_t*)data;
    ost->tmp_pkt->size=datalen;

    return write_h264_video_frame(ost,oc, ost->enc, ost->st, ost->tmp_pkt,&ost->frameIndex);
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    av_packet_free(&ost->tmp_pkt);
    if(ost->sws_ctx != nullptr) {
        sws_freeContext(ost->sws_ctx);
        swr_free(&ost->swr_ctx);
    }
    ost->bHaveIframe = 0;


}

int StreamWriterFfmpeg::openOutputStream(const char * url)
{
    int ret = 0;
    m_bInited = false;
    /* allocate the output media context */
    avformat_alloc_output_context2(&oc, NULL, NULL, url);
    if (!oc) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", url);
    }
    if (!oc)
        return 1;
    //((AVOutputFormat*)oc->oformat)->video_codec = AV_CODEC_ID_H264;
    fmt = oc->oformat;

    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (fmt->video_codec != AV_CODEC_ID_NONE) {

        add_stream(&video_st, oc, &video_codec, fmt->video_codec);
        have_video = 1;
        encode_video = 1;
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
        add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
        have_audio = 1;
        encode_audio = 1;
    }

    /* Now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */
    if (have_video)
        open_video(oc, video_codec, &video_st, opt);

    if (have_audio)
        open_audio(oc, audio_codec, &audio_st, opt);

    av_dump_format(oc, 0, url, 1);

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, url, AVIO_FLAG_WRITE);
        if (ret < 0) {
            // fprintf(stderr, "Could not open '%s': %s\n", url,
            //         av_err2str(ret));
            return 1;
        }
    }

    /* Write the stream header, if any. */
    ret = avformat_write_header(oc, &opt);
    if (ret < 0) {
        // fprintf(stderr, "Error occurred when opening output file: %s\n",
        //         av_err2str(ret));
        return 1;
    } 
    m_bInited = true;
    return 0;
}
int StreamWriterFfmpeg::writeOutputStream()
{
    // while (encode_video || encode_audio) {
    //     /* select the stream to encode */
    //     if (encode_video &&
    //         (!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
    //                                         audio_st.next_pts, audio_st.enc->time_base) <= 0)) {
    //         encode_video = !write_video_frame(oc, &video_st);
    //     } else {
    //         encode_audio = !write_audio_frame(oc, &audio_st);
    //     }
    // }
    return 0;
}
int StreamWriterFfmpeg::closeOutputStream()
{
    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    //此处有时候释放的时候会崩溃，先注释掉
    //av_write_trailer(oc);
    return 0;
}
int StreamWriterFfmpeg::freeOutputStream()
{
 /* Close each codec. */
    if (have_video)
        close_stream(oc, &video_st);
    if (have_audio)
        close_stream(oc, &audio_st);

    if (!(fmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_closep(&oc->pb);

    /* free the stream */
    avformat_free_context(oc);
    return 0;
}
int StreamWriterFfmpeg::sendRawVideoData(const uint8_t * data ,int datalen)
{
  int ret = 0;
  if(!m_bInited) {
    return 0;
  }
  m_mutex.lock();
  ret =  write_video_frame(oc, &video_st,data,datalen);
  m_mutex.unlock();
}
int StreamWriterFfmpeg::sendVideoData(const uint8_t * data ,int datalen)
{
  int ret = 0;
  if(!m_bInited) {
    return 0;
  }
  m_mutex.lock();
  ret =  write_video_frame(oc, &video_st,data,datalen);
  m_mutex.unlock();
}
int StreamWriterFfmpeg::sendRawAudioData(const uint8_t * data ,int datalen)
{
  int ret = 0;
  if(!m_bInited) {
    return 0;
  }
  m_mutex.lock();
  ret =  write_audio_frame(oc, &audio_st,data,datalen);
  m_mutex.unlock();
}
int StreamWriterFfmpeg::sendAudioData(const uint8_t * data ,int datalen)
{
  int ret = 0;
  if(!m_bInited) {
    return 0;
  }
  m_mutex.lock();
  ret =  write_audio_frame(oc, &audio_st,data,datalen);
  m_mutex.unlock();
}
