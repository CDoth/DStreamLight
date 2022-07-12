#include "DScaler.h"

using namespace DStreamLightNamespace;
//======================================================================================== DScalerData:
DScalerData::DScalerData():
    flags(0),
    pSrcFrame(nullptr),
    pDstFrame(nullptr),
    pSwsContext(nullptr),
    pDstBuffer(nullptr),
    iDstBufferSize(0),
    eSrcFormat(AV_PIX_FMT_NONE),
    eDstFormat(AV_PIX_FMT_NONE),
    ready(false),
    scaled(false),
    iDstWidth(0),
    iDstHeight(0)
{
}
DScalerData::~DScalerData() {
    freeAll();
}
int DScalerData::setScaler(int srcW, int srcH, AVPixelFormat srcFormat, AVPixelFormat dstFormat, int dstW, int dstH) {

    ready = false;
    freeAll();
    if( allocStorages() < 0 ) {
        DL_FUNCFAIL(1, "allocStorages");
        return -1;
    }
    dstW = dstW ? dstW : srcW;
    dstH = dstH ? dstH : srcH;
    eSrcFormat = srcFormat;
    eDstFormat = dstFormat;

    if(srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) {
        DL_BADVALUE(1, "srcW: [%d], srcH: [%d], dstW: [%d], dstH: [%d]", srcW, srcH, dstW, dstH);
        return -1;
    }
    if(eSrcFormat == AV_PIX_FMT_NONE || eDstFormat == AV_PIX_FMT_NONE) {
        DL_BADVALUE(1, "src format: [%d], dst format: [%d]", eSrcFormat, eDstFormat);
        return -1;
    }

    pSwsContext = sws_getContext (
                srcW, srcH, eSrcFormat, dstW, dstH, eDstFormat, flags,
                NULL, NULL, NULL
                );
    if(pSwsContext == NULL) {
        DL_FUNCFAIL(1, "sws_getContext");
        return -1;
    }
    int r = 0;
    if(pSrcFrame) {
        pSrcFrame->width = srcW;
        pSrcFrame->height = srcH;
        pSrcFrame->format = eSrcFormat;

        if( (r = av_image_alloc(pSrcFrame->data, pSrcFrame->linesize, srcW, srcH, eSrcFormat, 1)) < 0 ) {
            DL_FUNCFAIL(1, "av_image_alloc");
            freeAll();
            return r;
        }
    } else {
        DL_BADPOINTER(1, "pSrcFrame");
        freeAll();
        return -1;
    }

    if(pDstFrame) {
        pDstFrame->width = dstW;
        pDstFrame->height = dstH;
        pDstFrame->format = eDstFormat;
        if(  (r = av_frame_get_buffer(pDstFrame, 1)) < 0  ) {
            DL_FUNCFAIL(1, "av_frame_get_buffer");
            freeAll();
            return r;
        }
    } else {
        DL_BADPOINTER(1, "pDstFrame");
        freeAll();
        return -1;
    }

    size_t newBufferSize = av_image_get_buffer_size(eDstFormat, dstW, dstH, 1);
    if(newBufferSize != iDstBufferSize) {
        iDstBufferSize = newBufferSize;
        pDstBuffer = reget_zmem(pDstBuffer, iDstBufferSize);
    }
    iDstWidth = dstW;
    iDstHeight = dstH;

    ready = true;
    return 0;
}
uint8_t *DScalerData::scale(const uint8_t *image) {

    scaled = false;
    if(image == NULL) {
        DL_BADPOINTER(1, "image");
        return NULL;
    }
    if(eSrcFormat == AV_PIX_FMT_NONE || eDstFormat == AV_PIX_FMT_NONE) {
        DL_BADVALUE(1, "eSrcFormat: [%d] eDstFormat: [%d]", eSrcFormat, eDstFormat);
        return NULL;
    }

    int r = 0;
    if(pSrcFrame) {
        if( (r = av_image_fill_arrays(pSrcFrame->data, pSrcFrame->linesize, image, eSrcFormat, pSrcFrame->width, pSrcFrame->height, 1)) < 0 ) {
            DL_FUNCFAIL(1, "av_image_fill_arrays");
            return NULL;
        }
    }
    r = sws_scale(
                pSwsContext,
                pSrcFrame->data,
                pSrcFrame->linesize,
                0,
                pSrcFrame->height,
                pDstFrame->data,
                pDstFrame->linesize
                  );
    if(r < 0) {
        DL_FUNCFAIL(1, "sws_scale");
        return NULL;
    }
    r = av_image_copy_to_buffer(pDstBuffer, iDstBufferSize, pDstFrame->data, pDstFrame->linesize, eDstFormat, pDstFrame->width, pDstFrame->height, 1);
    if(r < 0) {
        DL_FUNCFAIL(1, "av_image_copy_to_buffer");
        return NULL;
    }



//    std::cout << "scaled to: eDstFormat: " << eDstFormat
//              << " from: " << eSrcFormat
//              << " src size: " << pSrcFrame->width << " " << pSrcFrame->height
//              << " dst size: " << pDstFrame->width << " " << pDstFrame->height
//              << std::endl;

    scaled = true;
    return pDstBuffer;
}
uint8_t *DScalerData::scale(const AVFrame *frame) {
    scaled = false;
        if(eSrcFormat == AV_PIX_FMT_NONE || eDstFormat == AV_PIX_FMT_NONE) {
            DL_BADVALUE(1, "eSrcFormat: [%d] eDstFormat: [%d]", eSrcFormat, eDstFormat);
            return NULL;
        }
        if(frame == NULL) {
            DL_BADPOINTER(1, "frame");
            return NULL;
        }
        int r = 0;
        r = sws_scale(
                    pSwsContext,
                    frame->data,
                    frame->linesize,
                    0,
                    frame->height,
                    pDstFrame->data,
                    pDstFrame->linesize
                      );
        if(r < 0) {
            DL_FUNCFAIL(1, "sws_scale");
            return NULL;
        }
        if( (r = av_image_copy_to_buffer(pDstBuffer, iDstBufferSize, pDstFrame->data, pDstFrame->linesize, eDstFormat, pDstFrame->width, pDstFrame->height, 1)) < 0 ) {
            DL_FUNCFAIL(1, "av_image_copy_to_buffer");
            return NULL;
        }


//        std::cout << "scaled to: eDstFormat: " << eDstFormat
//                  << " from: " << eSrcFormat
//                  << " src size: " << frame->width << " " << frame->height
//                  << " dst size: " << pDstFrame->width << " " << pDstFrame->height
//                  << std::endl;

        scaled = true;
        return pDstBuffer;
}
DRawFrame DScalerData::finalRawFrame() const {
    if(scaled) {
        DRawFrame frame;
        frame.data = pDstBuffer;
        frame.width = iDstWidth;
        frame.height = iDstHeight;
        frame.dataSize = iDstBufferSize;
        frame.pixFormat = eDstFormat;
        return frame;
    }
    return DRawFrame();
}
int DScalerData::allocStorages() {
    if( (pSrcFrame = av_frame_alloc()) == NULL ) {
        DL_BADALLOC(1, "pSrcFrame");
        return -1;
    }
    if( (pDstFrame = av_frame_alloc()) == NULL ) {
        DL_BADALLOC(1, "pDstFrame");
        return -1;
    }
    return 0;
}
void DScalerData::freeContext() {
    if(pSwsContext) {
        sws_freeContext(pSwsContext);
        pSwsContext = NULL;
    }
}
void DScalerData::freeAll() {
    clearFlags();
    freeContext();
    if(pSrcFrame) av_frame_free(&pSrcFrame);
    if(pDstFrame) av_frame_free(&pDstFrame);
    eSrcFormat = AV_PIX_FMT_NONE;
    eDstFormat = AV_PIX_FMT_NONE;
}
void DScalerData::clearFlags() {
    flags = 0;
}


//======================================================================================== DScaler:
DScaler::DScaler() {
    hold(new DScalerData);
}
DScaler::~DScaler(){
    release();
}
void DScaler::addFlag(int flag) {
    detach();
    data()->flags |= flag;
}
void DScaler::clearFlags(){
    detach();
    data()->clearFlags();
}
int DScaler::setScaler(int srcW, int srcH, AVPixelFormat srcFormat, AVPixelFormat dstFormat, int dstW, int dstH) {
    detach();
    return data()->setScaler(srcW, srcH, srcFormat, dstFormat, dstW, dstH);
}
uint8_t *DScaler::scale(const uint8_t *image){
    detach();
    return data()->scale(image);
}
uint8_t *DScaler::scale(const AVFrame *frame) {
    detach();
    return data()->scale(frame);
    return nullptr;
}
int DScaler::finalWidth() const {
    return data()->iDstWidth;
}
int DScaler::finalHeight() const {
    return data()->iDstHeight;
}
DFrameSize DScaler::finalSize() const {
    return {data()->iDstWidth, data()->iDstHeight};
}
DRawFrame DScaler::finalRawFrame() const {
    return data()->finalRawFrame();
}
uint8_t *DScaler::scaledData() const{
    if(data()->scaled)
        return data()->pDstBuffer;
    return nullptr;
}
int DScaler::scaledDataSize() const {
    return data()->iDstBufferSize;
}
AVFrame *DScaler::scaledFrame() const {
    if(data()->scaled) {
        return data()->pDstFrame;
    }
    return nullptr;
}
int DScaler::saveRawFrame(const char *path) const {
    if(data()->pDstBuffer == nullptr) {
        return -1;
    }
    if(data()->iDstBufferSize <= 0) {
        return -1;
    }
    FILE *f = fopen(path, "wb");
    if(f) {
        int w = fwrite(data()->pDstBuffer, data()->iDstBufferSize, 1, f);
        if( fclose(f) < 0 ) {
            DL_FUNCFAIL(1, "fclose");
        }
        return w;
    } else {
        DL_ERROR(1, "Can't open file: [%d]", path);
    }
    return -1;
}
AVPixelFormat DScaler::getDstFormat() const {return data()->eDstFormat;}
AVPixelFormat DScaler::getSrcFormat() const {return data()->eSrcFormat;}
bool DScaler::isScaled() const {return data()->scaled;}
bool DScaler::isReady() const { return data()->ready;   }
size_t DScaler::getBufferSize() const {return data()->iDstBufferSize;}
int DScaler::getFlags() const {return data()->flags;}

