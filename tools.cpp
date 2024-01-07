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
#include "tools.h"

FfmpegGlobal::FfmpegGlobal() {
    avformat_network_init();
}

FfmpegGlobal::~FfmpegGlobal() {
    avformat_network_deinit();
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

int find_sei(uint8_t *buf, int len)
{
    int pos = 0;
    int flag = 0;
    while (pos < len) {
        if (0 == buf[pos + 0] &&
            0 == buf[pos + 1] &&
            0 == buf[pos + 2] &&
            1 == buf[pos + 3]) {
            if ((buf[pos + 4] & 0x1f) == 6) {
                flag = 1;
                break;
            }
        }

        if (0 == buf[pos + 0] &&
            0 == buf[pos + 1] &&
            1 == buf[pos + 2]) {
            if ((buf[pos + 3] & 0x1f) == 6) {
                flag = 1;
                break;
            }
        }

        pos++;
    }

    if (flag) {
        return pos;
    }

    return 0;
}
int find_sps(uint8_t *buf, int len)
{
    int pos = 0;
    int flag = 0;
    while (pos < len) {
        if (0 == buf[pos + 0] &&
            0 == buf[pos + 1] &&
            0 == buf[pos + 2] &&
            1 == buf[pos + 3]) {
            if ((buf[pos + 4] & 0x1f) == 7) {
                flag = 1;
                break;
            }
        }

        if (0 == buf[pos + 0] &&
            0 == buf[pos + 1] &&
            1 == buf[pos + 2]) {
            if ((buf[pos + 3] & 0x1f) == 7) {
                flag = 1;
                break;
            }
        }

        pos++;
    }

    if (flag) {
        return pos;
    }

    return 0;
}

uint64_t u8bytes_to_u64(uint8_t *buff) {
    uint64_t frame_id = buff[7];
    frame_id = frame_id << 8 | buff[6];
    frame_id = frame_id << 8 | buff[5];
    frame_id = frame_id << 8 | buff[4];
    frame_id = frame_id << 8 | buff[3];
    frame_id = frame_id << 8 | buff[2];
    frame_id = frame_id << 8 | buff[1];
    frame_id = frame_id << 8 | buff[0];
    return frame_id;
}
