#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#include <cstdint>
#include <iostream>
#include <thread>
#include "pcm2aac.h"

using namespace std;
extern "C"
{
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int getAAC() {
    // 输入的PCM文件
    FILE *pInFile = fopen("/Users/faker/Desktop/code/ffmpeg/output.pcm", "rb");
    // 输出的AAC文件
    FILE *pOutFile = fopen("/Users/faker/Desktop/code/ffmpeg/output.aac", "ab");
    char aFrameBuf[1024] = {0}; // AVCodecContext->frame_size = 1024
    char *pOutData = NULL;
    int nReadSize = 0;
    int nOutSize = 0;
    Pcm2AAC objPcm2AAC;
    bool bEOF = false;

    // 我手头上的PCM是AV_SAMPLE_FMT_S16格式(有符号的16位), 采样率44100, 两声道的
    if (!(objPcm2AAC.Init(44100, AV_SAMPLE_FMT_S16, 2))) {
        printf("objPcm2AAC Init error\n");
        return -1;
    }

    printf("converting start\n");

    while (true) {
        nReadSize = fread(aFrameBuf, 1, sizeof(aFrameBuf), pInFile);
        if (nReadSize > 0) {
            // 把PCM数据放进编码器
            objPcm2AAC.AddData(aFrameBuf, nReadSize);
        } else {
            // 刷新编码器, 标志着PCM流结束
            objPcm2AAC.FlushData();
            bEOF = true;
        }

        while (true) {
            // 从编码器取出AAC数据
            if (!(objPcm2AAC.GetData(pOutData, nOutSize))) {
                break;
            }
            fwrite(pOutData, 1, nOutSize, pOutFile);
        }

        if (bEOF) {
            break;
        }
    }

    printf("converting end\n");

    fclose(pOutFile);
    fclose(pInFile);
    return 0;
}

int main(int argc, char *argv[]) {
//    getAAC();
    const AVOutputFormat *ofmt = NULL;
    int audio_fps = 0;
    int video_fps = 0;

    // Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex_v = -1, videoindex_out = -1;
    int audioindex_a = -1, audioindex_out = -1;
    int frame_index = 0;
    int64_t cur_pts_v = 0, cur_pts_a = 0;

    const char *in_filename_v = "/Users/faker/Desktop/code/ffmpeg/v.h264";
    const char *in_filename_a = "/Users/faker/Desktop/code/ffmpeg/v.aac";

    const char *out_filename = "/Users/faker/Desktop/code/ffmpeg/output.mp4";  // Output file URL

    // Input
    if ((ret = avformat_open_input(&ifmt_ctx_v, in_filename_v, 0, 0)) < 0) {
        printf("Could not open input file.");
        return 0;
//        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        return 0;
//        goto end;
    }

    if ((ret = avformat_open_input(&ifmt_ctx_a, in_filename_a, 0, 0)) < 0) {
        printf("Could not open input file.");
        return 0;
//        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx_a, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        return 0;
//        goto end;
    }
    printf("===========Input Information==========\n");
    av_dump_format(ifmt_ctx_v, 0, in_filename_v, 0);
    av_dump_format(ifmt_ctx_a, 0, in_filename_a, 0);
    printf("======================================\n");
    // Output
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        printf("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        return 0;
//        goto end;
    }
    ofmt = ofmt_ctx->oformat;

    for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
        // Create output AVStream according to input AVStream
        if (ifmt_ctx_v->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVStream *in_stream = ifmt_ctx_v->streams[i];
            AVCodecParameters *in_codecpar = in_stream->codecpar;

            AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
            if (!out_stream) {
                printf("Failed allocating output stream\n");
                return 0;
//                goto end;

            }

            if (avcodec_parameters_copy(out_stream->codecpar, in_codecpar) < 0) {
                printf("Failed to copy codec parameters\n");
                return 0;
//                goto end;
            }

            videoindex_v = i;
            videoindex_out = out_stream->index;
            // Allocate an AVCodecContext
            AVCodecContext *codec_ctx = avcodec_alloc_context3(NULL);
            if (!codec_ctx) {
                printf("Failed to allocate the AVCodecContext\n");
                return 0;
//                goto end;
            }
// Copy the settings of AVCodecParameters to AVCodecContext
            if (avcodec_parameters_to_context(codec_ctx, in_stream->codecpar) < 0) {
                printf("Failed to copy parameters to AVCodecContext\n");
                return 0;
//                goto end;
            }
            codec_ctx->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
                codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }
// Copy the settings of AVCodecContext to AVCodecParameters
            if (avcodec_parameters_from_context(out_stream->codecpar, codec_ctx) < 0) {
                printf("Failed to copy parameters from AVCodecContext\n");
                return 0;
//                goto end;
            }
            avcodec_free_context(&codec_ctx);
            break;
        }
    }

    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
        // Create output AVStream according to input AVStream
        if (ifmt_ctx_a->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            AVStream *in_stream = ifmt_ctx_a->streams[i];
            AVCodecParameters *in_codecpar = in_stream->codecpar;

            AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
            if (!out_stream) {
                printf("Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                return 0;
//                goto end;
            }

            if (avcodec_parameters_copy(out_stream->codecpar, in_codecpar) < 0) {
                printf("Failed to copy codec parameters\n");
                ret = AVERROR_UNKNOWN;
                return 0;
//                goto end;
            }

            audioindex_a = i;
            audioindex_out = out_stream->index;
// Allocate an AVCodecContext
            AVCodecContext *codec_ctx = avcodec_alloc_context3(NULL);
            if (!codec_ctx) {
                printf("Failed to allocate the AVCodecContext\n");
                return 0;
//                goto end;
            }
// Copy the settings of AVCodecParameters to AVCodecContext
            if (avcodec_parameters_to_context(codec_ctx, in_stream->codecpar) < 0) {
                printf("Failed to copy parameters to AVCodecContext\n");
                return 0;
//                goto end;
            }
            codec_ctx->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
                codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }
// Copy the settings of AVCodecContext to AVCodecParameters
            if (avcodec_parameters_from_context(out_stream->codecpar, codec_ctx) < 0) {
                printf("Failed to copy parameters from AVCodecContext\n");
                return 0;
//                goto end;
            }
            avcodec_free_context(&codec_ctx);
            break;
        }
    }

    printf("==========Output Information==========\n");
    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    printf("======================================\n");
    // Open output file
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
            printf("Could not open output file '%s'", out_filename);
            return 0;
//            goto end;
        }
    }
    // Write file header
    if (avformat_write_header(ofmt_ctx, NULL) < 0) {
        printf("Error occurred when opening output file\n");
        return 0;
//        goto end;
    }


    while (1) {
        AVFormatContext *ifmt_ctx;
        int stream_index = 0;
        AVStream *in_stream, *out_stream;
        cout << "video: " << cur_pts_v << " " << "audio: " << cur_pts_a << endl;
        // Get an AVPacket
        if (av_compare_ts(cur_pts_v, ifmt_ctx_v->streams[videoindex_v]->time_base, cur_pts_a, ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0) {
            ifmt_ctx = ifmt_ctx_v;
            stream_index = videoindex_out;

            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {

                    video_fps++;
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if (pkt.stream_index == videoindex_v) {
                        // FIX：No PTS (Example: Raw H.264)
                        // Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            // Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            // Duration between 2 frames (us)
                            cout << "-------------" << endl;
                            cout << "video time_base1: " << time_base1.num << " " << time_base1.den << endl;
                            cout << "video r_frame_rate: " << in_stream->r_frame_rate.num << " " << in_stream->r_frame_rate.den << endl;
                            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            cout << "video calc_duration: " << in_stream->r_frame_rate.num << " " << in_stream->r_frame_rate.den << endl;
                            // Parameters
                            pkt.pts = (double)(frame_index * calc_duration) / (double)(av_q2d(time_base1) * AV_TIME_BASE);
                            cout << "video pts: " << pkt.pts;
                            pkt.dts = pkt.pts;
                            pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1) * AV_TIME_BASE);
                            cout << "video duration: " << pkt.duration << endl;
                            cout << "-------------" << endl;
                            frame_index++;
                        }

                        cur_pts_v = pkt.pts;
                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            } else {
                break;
            }
        } else {
            ifmt_ctx = ifmt_ctx_a;
            stream_index = audioindex_out;
            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    audio_fps++;
//                    AVFrame *frame = av_frame_alloc();
//                    const AVCodec *codec;
//                    AVCodecContext *dec_ctx; // 假设已经初始化好解码器上下文

/* find the AAC decoder */
//                    codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
//                    dec_ctx = avcodec_alloc_context3(codec);
//                    avcodec_open2(dec_ctx, codec, NULL);
//                    avcodec_send_packet(dec_ctx, &pkt);

//                    ret = avcodec_receive_frame(dec_ctx, frame);
//                    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
//                         错误处理
//                    } else if (ret >= 0) {
//                        int nb_samples = frame->nb_samples;
//                        cout << nb_samples << endl;
                        // 现在你可以获取到音频帧中的采样数量
//                    }
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];
                    if (pkt.stream_index == audioindex_a) {
                        // FIX：No PTS
                        // Simple Write PTS

                        AVRational time_base1 = in_stream->time_base;
                        int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                        cout << "audio time_base1: " << time_base1.num << " " << time_base1.den << endl;
                        cout << "audio calc_duration: " << in_stream->r_frame_rate.num << " " << in_stream->r_frame_rate.den << endl;
                        cout << "audio duration: " << pkt.duration << endl;
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            // Write PTS
                            cout << "audio time_base1: " << time_base1.num << " " << time_base1.den << endl;
                            // Duration between 2 frames (us)
                            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            cout << "audio calc_duration: " << in_stream->r_frame_rate.num << " " << in_stream->r_frame_rate.den << endl;
                            // Parameters
                            pkt.pts = (double)(frame_index * calc_duration) / (double)(av_q2d(time_base1) * AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1) * AV_TIME_BASE);
                            frame_index++;
                        }
                        cout << "audio: " <<  in_stream->r_frame_rate.num << " " << in_stream->r_frame_rate.den << endl;
                        cur_pts_a = pkt.pts;
                        cout << "Faker audio" << endl;
                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            } else {
                break;
            }
        }
//        cout << in_stream->time_base.den << " "  << in_stream->time_base.num << endl;
        cout << "faker out"<< " " << out_stream->time_base.den << " " << out_stream->time_base.num << endl;
        cout << "faker out"<< " " << out_stream->r_frame_rate.den << " " << out_stream->r_frame_rate.num << endl;

        // Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index = stream_index;

        printf("Write 1 Packet. size:%5d\tpts:%lld\n", pkt.size, pkt.pts);
        // Write

        if (stream_index  == audioindex_out && av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
            printf("Error muxing packet\n");
            break;
        }
        av_packet_unref(&pkt);
    }

//    {
//        std::mutex mtx;
//        std::condition_variable cv;
//        bool ready = false;
//
//        std::thread t264([&]() {
//
//            AVFormatContext *ifmt_ctx;
//            int stream_index = 0;
//            AVStream *in_stream, *out_stream;
//            cout << "video: " << cur_pts_v << " " << "audio: " << cur_pts_a << endl;
//            // Get an AVPacket
//            ifmt_ctx = ifmt_ctx_v;
//            stream_index = videoindex_out;
//
//            while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
//
//                mtx.lock();
//
//                video_fps++;
//                in_stream = ifmt_ctx->streams[pkt.stream_index];
//                out_stream = ofmt_ctx->streams[stream_index];
//                if (pkt.stream_index == videoindex_v) {
//                    // FIX：No PTS (Example: Raw H.264)
//                    // Simple Write PTS
//                    if (pkt.pts == AV_NOPTS_VALUE) {
//                        // Write PTS
//                        AVRational time_base1 = in_stream->time_base;
//                        // Duration between 2 frames (us)
//                        cout << "-------------" << endl;
//                        int64_t calc_duration = (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
//                        cout << "video calc_duration: " << in_stream->r_frame_rate.num << " "
//                             << in_stream->r_frame_rate.den << endl;
//                        // Parameters
//                        pkt.pts = (double) (frame_index * calc_duration) / (double) (av_q2d(time_base1) * AV_TIME_BASE);
//                        cout << "video pts: " << pkt.pts;
//                        pkt.dts = pkt.pts;
//                        pkt.duration = (double) calc_duration / (double) (av_q2d(time_base1) * AV_TIME_BASE);
//                        cout << "video duration: " << pkt.duration << endl;
//                        cout << "-------------" << endl;
//                        frame_index++;
//
//
//                    }
//
//                    cur_pts_v = pkt.pts;
//                    pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
//                                               (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
//                    pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
//                                               (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
//                    pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
//                    pkt.pos = -1;
//                    pkt.stream_index = stream_index;
//
//                    printf("Write 1 Packet. size:%5d\tpts:%lld\n", pkt.size, pkt.pts);
//                    // Write
//                    if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
//                        printf("Error muxing packet\n");
//                        break;
//                    }
//                    av_packet_unref(&pkt);
//                    mtx.unlock();
//                }
//            }
//        });
//        std::thread taac([&]() {
//            AVFormatContext *ifmt_ctx;
//            int stream_index = 0;
//            AVStream *in_stream, *out_stream;
//            cout << "video: " << cur_pts_v << " " << "audio: " << cur_pts_a << endl;
//            // Get an AVPacket
//            ifmt_ctx = ifmt_ctx_a;
//            stream_index = audioindex_out;
//            while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
//                mtx.lock();
//                in_stream = ifmt_ctx->streams[pkt.stream_index];
//                out_stream = ofmt_ctx->streams[stream_index];
//                if (pkt.stream_index == audioindex_a) {
//                    if (pkt.pts == AV_NOPTS_VALUE) {
//                        // Write PTS
//                        AVRational time_base1 = in_stream->time_base;
//                        // Duration between 2 frames (us)
//                        int64_t calc_duration = (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
//                        // Parameters
//                        pkt.pts = (double) (frame_index * calc_duration) /
//                                  (double) (av_q2d(time_base1) * AV_TIME_BASE);
//                        pkt.dts = pkt.pts;
//                        pkt.duration = (double) calc_duration / (double) (av_q2d(time_base1) * AV_TIME_BASE);
//                        frame_index++;
//                    }
//                    cout << "audio: " << in_stream->r_frame_rate.num << " " << in_stream->r_frame_rate.den << endl;
//                    cur_pts_a = pkt.pts;
//                    cout << "Faker audio" << endl;
//
//
//                    pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
//                                               (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
//                    pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
//                                               (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
//                    pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
//                    pkt.pos = -1;
//                    pkt.stream_index = stream_index;
//
//                    printf("Write 1 Packet. size:%5d\tpts:%lld\n", pkt.size, pkt.pts);
//
//                    // Write
//                    if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
//                        printf("Error muxing packet\n");
//                        break;
//                    }
//
//
//                    av_packet_unref(&pkt);
//                    mtx.unlock();
//                }
//            }
//
//        });
//
//        t264.join();
//        taac.join();
//    }
//    while (1) {
//        AVFormatContext *ifmt_ctx;
//        int stream_index = 0;
//        AVStream *in_stream, *out_stream;
//        cout << "video: " << cur_pts_v << " " << "audio: " << cur_pts_a << endl;
//        // Get an AVPacket
//        if (av_compare_ts(cur_pts_v, ifmt_ctx_v->streams[videoindex_v]->time_base, cur_pts_a,
//                          ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0) {
//            ifmt_ctx = ifmt_ctx_v;
//            stream_index = videoindex_out;
//
//            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
//                do {
//
//                    video_fps++;
//                    in_stream = ifmt_ctx->streams[pkt.stream_index];
//                    out_stream = ofmt_ctx->streams[stream_index];
//
//                    if (pkt.stream_index == videoindex_v) {
//                        // FIX：No PTS (Example: Raw H.264)
//                        // Simple Write PTS
//                        if (pkt.pts == AV_NOPTS_VALUE) {
//                            // Write PTS
//                            AVRational time_base1 = in_stream->time_base;
//                            // Duration between 2 frames (us)
//                            cout << "-------------" << endl;
//                            int64_t calc_duration = (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
//                            cout << "video calc_duration: " << in_stream->r_frame_rate.num << " "
//                                 << in_stream->r_frame_rate.den << endl;
//                            // Parameters
//                            pkt.pts = (double) (frame_index * calc_duration) /
//                                      (double) (av_q2d(time_base1) * AV_TIME_BASE);
//                            cout << "video pts: " << pkt.pts;
//                            pkt.dts = pkt.pts;
//                            pkt.duration = (double) calc_duration / (double) (av_q2d(time_base1) * AV_TIME_BASE);
//                            cout << "video duration: " << pkt.duration << endl;
//                            cout << "-------------" << endl;
//                            frame_index++;
//                        }
//
//                        cur_pts_v = pkt.pts;
//                        break;
//                    }
//                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
//            } else {
//                break;
//            }
//        } else {
//            ifmt_ctx = ifmt_ctx_a;
//            stream_index = audioindex_out;
//            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
//                do {
//                    audio_fps++;
////                    AVFrame *frame = av_frame_alloc();
////                    const AVCodec *codec;
////                    AVCodecContext *dec_ctx; // 假设已经初始化好解码器上下文
//
///* find the AAC decoder */
////                    codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
////                    dec_ctx = avcodec_alloc_context3(codec);
////                    avcodec_open2(dec_ctx, codec, NULL);
////                    avcodec_send_packet(dec_ctx, &pkt);
//
////                    ret = avcodec_receive_frame(dec_ctx, frame);
////                    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
////                         错误处理
////                    } else if (ret >= 0) {
////                        int nb_samples = frame->nb_samples;
////                        cout << nb_samples << endl;
//                    // 现在你可以获取到音频帧中的采样数量
////                    }
//                    in_stream = ifmt_ctx->streams[pkt.stream_index];
//                    out_stream = ofmt_ctx->streams[stream_index];
//                    if (pkt.stream_index == audioindex_a) {
//                        // FIX：No PTS
//                        // Simple Write PTS
//                        if (pkt.pts == AV_NOPTS_VALUE) {
//                            // Write PTS
//                            AVRational time_base1 = in_stream->time_base;
//                            // Duration between 2 frames (us)
//                            int64_t calc_duration = (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
//                            // Parameters
//                            pkt.pts = (double) (frame_index * calc_duration) /
//                                      (double) (av_q2d(time_base1) * AV_TIME_BASE);
//                            pkt.dts = pkt.pts;
//                            pkt.duration = (double) calc_duration / (double) (av_q2d(time_base1) * AV_TIME_BASE);
//                            frame_index++;
//                        }
//                        cout << "audio: " << in_stream->r_frame_rate.num << " " << in_stream->r_frame_rate.den
//                             << endl;
//                        cur_pts_a = pkt.pts;
//                        cout << "Faker audio" << endl;
//                        break;
//                    }
//                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
//            } else {
//                break;
//            }
//        }
////        cout << in_stream->time_base.den << " "  << in_stream->time_base.num << endl;
//        cout << stream_index << " " << out_stream->r_frame_rate.den << " " << out_stream->r_frame_rate.num << endl;
//
//        // Convert PTS/DTS
//        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
//                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
//        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
//                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
//        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
//        pkt.pos = -1;
//        pkt.stream_index = stream_index;
//
//        printf("Write 1 Packet. size:%5d\tpts:%lld\n", pkt.size, pkt.pts);
//        // Write
//        if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
//            printf("Error muxing packet\n");
//            break;
//        }
//        av_packet_unref(&pkt);
//    }
//
//


    // Write file trailer
    av_write_trailer(ofmt_ctx);

    end:
    avformat_close_input(&ifmt_ctx_v);
    avformat_close_input(&ifmt_ctx_a);
    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE)) avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        printf("Error occurred.\n");
        return -1;
    }
    cout << "fps: " << video_fps << " " << audio_fps;
    return 0;
}
