#include "DStreamProcessor.h"

//======================================================================== DEncoder:
DEncoder::DEncoder() {
    deviceReader = nullptr;
    encoder = nullptr;
    captureMaster = nullptr;
}
void DEncoder::proc() {
    __capture();
    __encode();
}
void DEncoder::__capture() {
    if(captureMaster) {
        captureMaster->capture();
    } else {
        deviceReader->read();
    }
}
void DEncoder::__encode() {
    encoder->encode(deviceReader->readedData());
}
