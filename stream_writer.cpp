
#include "stream_writer.h"
#include "ffmpeg_writer.h"
#include "tools.h"



std::shared_ptr<StreamWriter> StreamWriter::create(int type)
{
    std::shared_ptr<StreamWriter> ptr;
    switch (type) {
    case 0:
        ptr.reset(new StreamWriterFfmpeg());
        break;
    }

    return ptr;

}

//class VideoObserver :public NetStreamWriterObserver
//{
//public:
//    VideoObserver(){}
//    ~VideoObserver(){}
//
//    void OnDecodedFrame(const AVFrame *pFrame) override {
//        std::cout << "w=" << pFrame->width
//            << ",h=" << pFrame->height
//            << std::endl;
//    }
//};
//
//
//
//int main(int argc, char *argv[])
//{
//    if (argc <= 1) {
//        std::cout << "missing parameter: rtsp URL" << std::endl;
//        return 0;
//    }
//
//    std::shared_ptr<NetStreamWriter> reader(NetStreamWriter::create(0));
//    VideoObserver observer;
//    int ret = reader->OpenStream(argv[1], &observer);
//    if (ret < 0) {
//        return 0;
//    }
//
//    while (1) {
//        std::this_thread::sleep_for(std::chrono::seconds(10));
//    }
//
//
//}
