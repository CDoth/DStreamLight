#ifndef DFFMPEG_H
#define DFFMPEG_H
#include "DScaler.h"


struct InnerWriteContext {
    uint8_t *buffer;
    int bufferSize;
    int bufferCapacity;
};
struct InnerReadContext {
    const uint8_t *start;
    const uint8_t *end;
};
enum DFFMpegState {
    DFF__EMPTY
    ,DFF__ENCODER
    ,DFF__DECODER
    ,DFF__ERROR
};
enum DFFmpegType {
    DFF__NO_TYPE
    ,DFF__AUDIO
    ,DFF__VIDEO
};
class DFFMpeg {
public:
    DFFMpeg();
    bool initVideoEncoder(const char *format, AVCodecID codecID, int width, int height, AVPixelFormat rawFormat = AV_PIX_FMT_NONE);
    bool initVideoEncoder(const char *format, AVCodecID codecID, const char *deviceOpener, const char *deviceName);
    bool initVideoDecoder(AVPixelFormat preferredFormat = AV_PIX_FMT_NONE);

    bool initAudioEncoder(AVCodecID codecID, const char *deviceOpener, const char *deviceName);
    bool initAudioDecoder();

    bool initShotEncoder(AVCodecID codecID);

    void stop();
    bool deviceRestartLast();
    bool deviceStop();
    bool devicePause();

    inline bool isEmpty() const {return eState == DFF__EMPTY;}
    inline bool isEncoder() const {return eState == DFF__ENCODER;}
    inline bool isDecoder() const {return eState == DFF__DECODER;}
    inline bool isFault() const {return eState == DFF__ERROR;}
public:
    bool encode(const uint8_t *rawFrameData);
    bool encode(AVFrame *frame);
    bool read();
    bool encode();
    bool encodeImage();

    struct timeval encodingTime() const;
    int encodingSeconds() const;
public:
    bool setVideoData(const uint8_t *data, size_t size);
    void copyVideoData(const uint8_t *data, size_t size);
    void appendVideoData(const uint8_t *data, size_t size);
    bool decode();
public:
    const uint8_t *shotData() const;
    int shotSize() const;

    int writtenSize() const;
    const uint8_t *writtenData() const;
    void clearWritten();

    const uint8_t * readedData() const;
    int readedSize() const;

    const uint8_t * decodedData() const;
    int decodedSize() const;
private:
    int __setOption(const char *key, const char *value, int flags);
    void __clear();
    void __freeAll();
    bool __createWriteContext();
    void __freeWriteContext();

    bool __createReadContext();
    void __freeReadContext();
    bool __openInputStream();
private:
    int64_t __getPts();
    bool __encodeVideo(AVFrame *frame);
    bool __encodeImage(AVFrame *frame);
    bool __encodeAudio();
    bool __writeHeader();
    bool __writePacket();
private:
    DFFMpegState eState;
    DFFmpegType eType;
    int iErrorCode;

private: //common:
    AVFormatContext *pFormatContext;
    AVCodecContext *pCodecContext;
    AVDictionary *pOptions;
    AVFrame *pFrame;
    AVPacket *pPacket;
    AVIOContext *pAVIOContext;
    const AVCodec *pCodec;
    uint8_t *pAVIOBuffer;
    int iAVIOBufferSize;
    const char *pFormat;
    DScaler wScaler;
    bool iNeedScaling;
private: //shot:
    const AVCodec *__shot__pCodec;
    AVCodecContext *__shot__pCodecContext;
    AVFrame *__shot__pFrame;
    DScaler __shot__wScaler;
    bool __shot__iNeedScaling;
    DArray<uint8_t> __shot__buffer;
private: //device:
    AVFormatContext *device__pFormatContext;
    const AVInputFormat *device__pInputFormat;
    AVPacket *device__pPacket;

    bool device__iLastInited;
    AVCodecID device__iLastCodec;
    std::string device__iLastFormat;
    std::string device__iLastOpener;
    std::string device__iLastName;
private: //encode:
    struct timeval __encode__start_time = {0,0};
    struct timeval __encode__current_time = {0,0};
    int __encode__gop_counter;
    int __encode__bframes_counter;
    bool __encode__auto_writing;
    InnerWriteContext *__encode__write_context;
    AVPixelFormat __encode__raw_format;

    static const AVRational __encode__longConst;
    static const AVRational __encode__T;

    DArray<uint8_t> __encode__stream_header;
    DArray<uint8_t> __encode__audio_buffer;
    bool __encode__first_frame;

private: //decode:
    InnerReadContext *__decode__read_context;
    AVPixelFormat __decode__preferred_format;
    AVPixelFormat __decode__decoded_format;
    bool __decode__is_stream_open;
    DArray<uint8_t> __decode__complete_data;
};

#endif // DFFMPEG_H
