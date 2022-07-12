#ifndef DCAPTUREMASTER_H
#define DCAPTUREMASTER_H
#include "dstreamlight.h"

class DCaptureMaster {
public:
    DCaptureMaster();
    int capture(void *base = nullptr, uint8_t *captureBuffer = nullptr, long captureBufferSize = 0);
};

#endif // DCAPTUREMASTER_H
