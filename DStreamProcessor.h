#ifndef DSTREAMPROCESSOR_H
#define DSTREAMPROCESSOR_H

#include "DFFMpeg.h"
#include "DScaler.h"
#include "DCaptureMaster.h"


class DEncoder {
public:
    DEncoder();

    void proc();
private:
    void __capture();
    void __encode();
private:
    DCaptureMaster *captureMaster;
    DFFMpeg *deviceReader;

    DFFMpeg *encoder;
};
class DDecoder {
public:
private:
};

#endif // DSTREAMPROCESSOR_H
