#include "stream_reader.h"
#include "ffmpeg_reader.h"
#include "tools.h"
static int ffmpeg_interrupt_cb(void* p)
{
    //printf("#### ERROR ####\n");
    return 0;
}
template< typename... Args >
std::string vformat(const char* format, Args... args)
{
    size_t length = std::snprintf(nullptr, 0, format, args...);
    if (length <= 0)
    {
        return "";
    }

    char* buf = new char[length + 1];
    std::snprintf(buf, length + 1, format, args...);

    std::string str(buf);
    delete[] buf;
    return std::move(str);
}

StreamReaderFfmpeg::StreamReaderFfmpeg() {
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
    
    
}

StreamReaderFfmpeg::~StreamReaderFfmpeg() {
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
void StreamReaderFfmpeg::initTestFile()
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
void StreamReaderFfmpeg::OpenCodecContext(AVMediaType type, AVCodecContext **codec_context, int *stream_index) const {
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
int StreamReaderFfmpeg::DecodePacket(AVCodecContext *codec_context) {
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
int StreamReaderFfmpeg::OutputAudioFrame(AVFrame *frame) {
    
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


int StreamReaderFfmpeg::OutputVideoFrame(AVFrame *frame) {
    
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

int StreamReaderFfmpeg::OpenStream(std::string strUrl, StreamReaderObserver *observer)
{
    
    initTestFile();
    
   
    
    m_observer = observer;
    AVDictionary* options = NULL;
    if (strUrl.find("rtsp://") != std::string::npos) {
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
        av_dict_set(&options, "fflags", "nobuffer", 0);
        av_dict_set(&options, "protocol_whitelist", "file,http,https,rtp,udp,tcp,tls", 0);
    }
    av_dict_set(&options, "stream_loop", "-1", 0); // 无限循环播放,代码下不起作用，要自己做循环打开
    //av_dict_set(&options, "r", "30", 0);
    int err = avformat_open_input(&m_pFormatCtx, strUrl.c_str(), NULL, &options);
    if (err < 0)
    {
        std::cout << "Can not open this file " << strUrl << std::endl;
        return -1;
    }

    av_dict_free(&options);

    err = avformat_find_stream_info(m_pFormatCtx, NULL);
    if (err < 0)
    {
        std::cout << "Unable to get stream info";
        return -1;
    }
   
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    for (unsigned int i = 0; i < m_pFormatCtx->nb_streams; i++)
    {
        if (m_pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_videoStreamIndex = i;
           // break;
        }else
        if (m_pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            m_audioStreamIndex = i;
        //    break;
        }
    }

    if (m_videoStreamIndex == -1)
    {
        std::cout << "Unable to find video stream";
        return -1;
    }

    //m_pCodecCtx = m_pFormatCtx->streams[m_videoStreamIndex]->codec;
    m_fps = m_pFormatCtx->streams[m_videoStreamIndex]->avg_frame_rate.num/m_pFormatCtx->streams[m_videoStreamIndex]->avg_frame_rate.den;//每秒多少帧

    //avpicture_alloc(&m_pAVPicture, AV_PIX_FMT_RGB24, m_pCodecCtx->width, m_pCodecCtx->height);
#if 0
    const AVCodec *pCodec = avcodec_find_decoder(m_pFormatCtx->streams[m_videoStreamIndex]->codecpar->codec_id);;
    
    m_pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!m_pCodecCtx) {
       av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", m_videoStreamIndex);
       return AVERROR(ENOMEM);
    }
    int ret = avcodec_parameters_to_context(m_pCodecCtx, m_pFormatCtx->streams[m_videoStreamIndex]->codecpar);
    if (ret < 0) {
         av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
                      "for stream #%u\n", m_videoStreamIndex);
         return ret;
    }
    //std::cout << QString("video size : width=%d height=%d \n").arg(pCodecCtx->width).arg(pCodecCtx->height);
    if (pCodec == NULL)
    {
        std::cout << "Unsupported codec";
        return -1;
    }
    if (avcodec_open2(m_pCodecCtx, pCodec, NULL) < 0)
    {
        std::cout << "Unable to open codec";
        return -1;
    }
#endif
    //https://blog.csdn.net/a992036795/article/details/123059880
    OpenCodecContext(AVMEDIA_TYPE_VIDEO,
                         &this->m_pVideoCodecCtx, &this->m_videoStreamIndex);
    OpenCodecContext(AVMEDIA_TYPE_AUDIO,
                         &this->m_pAudioCodecCtx, &this->m_audioStreamIndex);
    if(!m_bNotDecoder) {
        AllocImage();
    }
    m_width = m_pVideoCodecCtx->width;
    m_height = m_pVideoCodecCtx->height;

    m_pSwsCtx = sws_getContext(m_width, m_height, AV_PIX_FMT_YUV420P, m_width, m_height, AV_PIX_FMT_RGB24, SWS_BICUBIC, 0, 0, 0);
   

   
    avcodec_parameters_copy(m_bsf_ctx->par_in, m_pFormatCtx->streams[m_videoStreamIndex]->codecpar);
    av_bsf_init(m_bsf_ctx);
    std::cout << "initial successfully";
    m_state = StreamStateStarted;
    // Decoder thread
    m_decodeThread = new std::thread([=] {
        int ret = 0;
        int got_pictrue = 0;
        uint8_t sei_buf[256];
        int sei_len = 0;
        while (true) {
            if (m_state == StreamStateStopped) {
                break;
            }
            //av_log(NULL, AV_LOG_INFO, "start av_read_frame\n");
            ret = av_read_frame(m_pFormatCtx, &m_packet);
            if (ret < 0) {
                //std::this_thread::sleep_for(std::chrono::milliseconds() * 10);
                //循环播放视频
                 ret = avformat_seek_file(m_pFormatCtx, -1, INT64_MIN, m_pFormatCtx->start_time, m_pFormatCtx->start_time, 0);
                 continue;
            }

           
            // handle video stream
            if (m_packet.stream_index == m_videoStreamIndex)
            {
                //mp4文件和h264文件读取的包内容都不一样
                //https://blog.csdn.net/heng615975867/article/details/120026185
                //av_log(NULL, AV_LOG_INFO, "end av_read_frame\n");
                if(m_bNotDecoder == true) {
                    bool finalPktRead=false;
                    int finalPacketWritten = 0;
                    int ret = 0;
                    ret = av_bsf_send_packet(m_bsf_ctx, &m_packet);
                    if(ret < 0)  {
                        //qDebug("av_bsf_send_packet error");
                        continue;
                    }
                    while ((ret = av_bsf_receive_packet(m_bsf_ctx, &m_packet)) == 0) {
                          //      fwrite(packet->data, packet->size, 1, fp);
                        int pos = 0;
                        if ((m_packet.data[4]  & 0x1f) == 6) {
                            pos = find_sps(m_packet.data,m_packet.size);
                        }
                       
                       
                        if (m_observer != nullptr) {
                            m_observer->OnVideoRawFrame(m_packet.data+pos,m_packet.size-pos,m_width,m_height,m_fps);
                        }
                    }
                   
                    av_packet_unref(&m_packet);
                   // av_usleep(10*1000);
                    getSendOneFrameDelay(1000/m_fps);
                    continue;
                }
#if 0
                //for decoder yuv
                ret = avcodec_send_packet(m_pVideoCodecCtx, &m_packet);
                if (ret < 0) {
                    std::cout << "avcodec_send_packet error=" << ret << std::endl;
                }

                ret = avcodec_receive_frame(m_pVideoCodecCtx, m_pFrame);
                if (ret < 0) {
                    std::cout << "avcodec_send_packet error=" << ret << std::endl;
                }
                else {
                    if (m_observer != nullptr) {
                        m_observer->OnVideoDecodedFrame(m_pFrame->data[0], m_pFrame->linesize[0],m_pFrame->width, m_pFrame->height);
                        sei_len = 0;
                    }
                }
#endif
                DecodePacket(this->m_pVideoCodecCtx);
            } else {
                //for audio
                // decode audio data
                DecodePacket(this->m_pAudioCodecCtx);
            }
            av_packet_unref(&m_packet);
            //getSendOneFrameDelay(1000/30);
        }
    });


    return 0;

}
void StreamReaderFfmpeg::AllocImage() {

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


int StreamReaderFfmpeg::CloseStream()
{
    m_state = StreamStateStopped;
    m_decodeThread->join();
    delete m_decodeThread;
    av_bsf_free(&m_bsf_ctx);
    return 0;
}
void StreamReaderFfmpeg::SetNotDecoder(bool on)
{
    m_bNotDecoder = on;
}
int StreamReaderFfmpeg::getSendOneFrameDelay(int64_t frameTime)

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
int StreamReaderFfmpeg::get_format_from_sample_fmt(const char **fmt,
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
void StreamReaderFfmpeg::fill_samples(double *dst, int nb_samples, int nb_channels, int sample_rate, double *t)
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
int StreamReaderFfmpeg::reSample(int64_t src_ch_layout,int src_rate,uint8_t **src_data,int src_nb_channels, AVSampleFormat src_sample_fmt,int src_nb_samples,
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
            //printf("t:%f in:%d out:%d\n", t, src_nb_samples, ret);
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
