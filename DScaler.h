#ifndef DSCALER_H
#define DSCALER_H
#include "dstreamlight.h"

class DScalerData {
    friend class DScaler;
public:
    DScalerData();
    ~DScalerData();
    int setScaler(int srcW, int srcH, AVPixelFormat srcFormat, AVPixelFormat dstFormat, int dstW = 0, int dstH = 0);
    uint8_t *scale(const uint8_t *image);
    uint8_t *scale(const AVFrame *frame);

    DRawFrame finalRawFrame() const;

private:
    int allocStorages();
    void freeContext();
    void freeAll();
    void clearFlags();

private:
    int flags;
    AVFrame *pSrcFrame;
    AVFrame *pDstFrame;
    SwsContext *pSwsContext;
    uint8_t *pDstBuffer;
    size_t iDstBufferSize;
    AVPixelFormat eSrcFormat;
    AVPixelFormat eDstFormat;
    int iDstWidth;
    int iDstHeight;


    bool ready;
    bool scaled;
};
class DScaler : public DWatcher<DScalerData> {
public:
    DScaler();
    ~DScaler();

    void addFlag(int flag);
    void clearFlags();
    int setScaler(int srcW, int srcH, AVPixelFormat srcFormat, AVPixelFormat dstFormat, int dstW = 0, int dstH = 0);
    uint8_t* scale(const uint8_t *image);
    uint8_t* scale(const AVFrame *frame);

    int finalWidth() const;
    int finalHeight() const;
    DFrameSize finalSize() const;
    DRawFrame finalRawFrame() const;

    uint8_t* scaledData() const;
    int scaledDataSize() const;
    AVFrame* scaledFrame() const;
    int saveRawFrame(const char *path) const;
    AVPixelFormat getDstFormat() const;
    AVPixelFormat getSrcFormat() const;
    bool isScaled() const;
    bool isReady() const;
    size_t getBufferSize() const;
    int getFlags() const;
};
#endif // DSCALER_H
