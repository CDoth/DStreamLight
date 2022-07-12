#ifndef DSTREAMLIGHT_H
#define DSTREAMLIGHT_H

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>

#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include "libavutil/pixdesc.h"

#include <libavcodec/avfft.h>
}
#include <DLogs.h>

void registerAll();
namespace DStreamLightNamespace {
    extern DLogs::DLogsContext log_context;
    extern DLogs::DLogsContextInitializator logsInit;
}
struct DRawFrame {
    DRawFrame() { zero_mem(this, sizeof(DRawFrame)); pixFormat = AV_PIX_FMT_NONE; }
    ~DRawFrame() { zero_mem(this, sizeof(DRawFrame)); }
    uint8_t *data;
    size_t dataSize;
    int width;
    int height;
    AVPixelFormat pixFormat;
};
struct DFrameSize {
    DFrameSize() : width(0), height(0) {}
    DFrameSize(int w, int h) : width(w), height(h) {}
    bool operator==(const DFrameSize &s) {
        return (width == s.width) && (height == s.height);
    }
    int width;
    int height;
};

#endif // DSTREAMLIGHT_H
