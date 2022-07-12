#include "DFFMpeg.h"
#include <sys/time.h>
#include "DProfiler.h"
using namespace DStreamLightNamespace;
const AVRational DFFMpeg::__encode__longConst = {1, 1000000L};
const AVRational DFFMpeg::__encode__T = {1, 1000};

DFFMpeg::DFFMpeg() {
    __clear();
}
bool DFFMpeg::initVideoEncoder(const char *format, AVCodecID codecID, int width, int height, AVPixelFormat rawFormat) {

    if(format == nullptr) {
        DL_BADPOINTER(1, "format");
        return false;
    }
    if(codecID == AV_CODEC_ID_NONE) {
        DL_BADVALUE(1, "codec ID: [%d]", codecID);
        return false;
    }
    if(width <= 0 || height <= 0) {
        DL_BADVALUE(1, "width: [%d], height: [%d]", width, height);
        return false;
    }

    if(eState != DFF__EMPTY)
        __freeAll();


    AVStream *pStream = nullptr;
    //--------------------------------------------------------------------
    if( (pFormatContext = avformat_alloc_context()) == NULL ) {
        DL_FUNCFAIL(1, "avformat_alloc_context");
        goto fail;
    }
    if( (pFormatContext->oformat = av_guess_format(format, NULL, NULL)) == NULL ) {
        DL_FUNCFAIL(1, "av_guess_format");
        goto fail;
    }
    if( (pCodec = const_cast<AVCodec*>(avcodec_find_encoder(codecID))) == NULL ) {
        DL_FUNCFAIL(1, "avcodec_find_encoder");
        goto fail;
    }
    if( (pCodecContext = avcodec_alloc_context3(pCodec)) == NULL ) {
        DL_FUNCFAIL(1, "avcodec_alloc_context3");
        goto fail;
    }
    pCodecContext->codec_id = codecID;

    //--------------------------------------------------------------------
    if( __setOption("preset", "ultrafast", 0) < 0) {DL_FUNCFAIL(1, "setOption: preset"); goto fail;}
    if( __setOption("tune", "zerolatency", 0) < 0) {DL_FUNCFAIL(1, "setOption: tune"); goto fail;}
    if( __setOption("crf", "30", 0) < 0 ) {DL_FUNCFAIL(1, "setOption: crf"); goto fail;}

    //--------------------------------------------------------------------
    if( pCodec->pix_fmts ) {
        if(rawFormat == AV_PIX_FMT_NONE) {
            pCodecContext->pix_fmt = pCodec->pix_fmts[0];
        } else  {
            for(int i=0;pCodec->pix_fmts[i] != AV_PIX_FMT_NONE; ++i) {
                if(pCodec->pix_fmts[i] == rawFormat) {
                    pCodecContext->pix_fmt = rawFormat;
                    break;
                }
            }
            if(pCodecContext->pix_fmt != rawFormat)
                pCodecContext->pix_fmt = pCodec->pix_fmts[0];
        }
    }
    __encode__raw_format = rawFormat;

    //--------------------------------------------------------------------
    pCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    pCodecContext->width = width;
    pCodecContext->height = height;
    pCodecContext->time_base.num = 1;
    pCodecContext->time_base.den = 30;

    pCodecContext->gop_size = 30;
    pCodecContext->bit_rate = 400 * 1000;
    pCodecContext->max_b_frames = 0;

    pCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    pFormatContext->flags |= AVFMT_FLAG_GENPTS;
    if(pCodecContext->codec_id == AV_CODEC_ID_H264) pCodecContext->profile = FF_PROFILE_H264_HIGH;


    if( (pStream = avformat_new_stream(pFormatContext, NULL)) == NULL ) {
        DL_FUNCFAIL(1, "avformat_new_stream");
        goto fail;
    }
    if( (iErrorCode = avcodec_open2(pCodecContext, pCodec, &pOptions)) < 0 ) {
        DL_FUNCFAIL(1, "avcodec_open2: [%d]", iErrorCode);
        goto fail;
    }
    if(pStream == NULL || pStream->codecpar == NULL || pCodecContext == NULL) {
        DL_BADPOINTER(1, "pStream: [%p], codecpar: [%p], pCodecContext: [%p]", pStream, pStream->codecpar, pCodecContext);
        goto fail;
    }
    if( (iErrorCode = avcodec_parameters_from_context(pStream->codecpar, pCodecContext)) < 0) {
        DL_FUNCFAIL(1, "avcodec_parameters_from_context: [%d]", iErrorCode);
        goto fail;
    }


    pStream->time_base.num = 1;
    pStream->time_base.den = 30;
    //--------------------------------------------------------------------
    pFrame = av_frame_alloc();
    pPacket = av_packet_alloc();

    pFrame->width = width;
    pFrame->height = height;
    pFrame->format = pCodecContext->pix_fmt;
    pFormat = format;
    //--------------------------------------------------------------------

    DL_INFO(1, "source fomrat: [%d] encode format: [%d]", rawFormat, pCodecContext->pix_fmt);

    if(pCodecContext->pix_fmt != rawFormat) {
        wScaler.setScaler(width, height, rawFormat, pCodecContext->pix_fmt);
        if(wScaler.isReady() == false) {
            DL_ERROR(1, "Can't prepare scaler");
            goto fail;
        }
        iNeedScaling = true;
    }
    //--------------------------------------------------------------------
    if( __createWriteContext() == false ) {
        DL_FUNCFAIL(1, "__createWriteContext");
        goto fail;
    }
    //--------------------------------------------------------------------



    eState = DFF__ENCODER;
    eType = DFF__VIDEO;
    return true;
fail:
    __freeAll();
    return false;
}
bool DFFMpeg::initVideoEncoder(const char *format, AVCodecID codecID, const char *deviceOpener, const char *deviceName) {

    if(deviceOpener == nullptr || deviceName == nullptr) {
        DL_BADPOINTER(1, "deviceOpener: [%p] deviceName: [%p]", deviceOpener, deviceName);
        return false;
    }
    __freeAll();

    AVStream *pStream = nullptr;

    // read device:
    if( (device__pFormatContext = avformat_alloc_context()) == NULL ) {
        DL_FUNCFAIL(1, "avformat_alloc_context (0)");
        goto fail;
    }
    if( (device__pInputFormat = av_find_input_format(deviceOpener)) == nullptr ) {
        DL_FUNCFAIL(1, "av_find_input_format");
        goto fail;
    }
    if( (iErrorCode = avformat_open_input(&device__pFormatContext, deviceName, device__pInputFormat, NULL)) < 0 ) {
        DL_FUNCFAIL(1, "avformat_open_input: [%d]", iErrorCode);
        goto fail;
    }
    if( (iErrorCode = avformat_find_stream_info(device__pFormatContext, NULL)) < 0 ) {
        DL_FUNCFAIL(1, "avformat_find_stream_info: [%d]", iErrorCode);
        goto fail;
    }
    if(device__pFormatContext->nb_streams == 0) {
        DL_ERROR(1, "No streams");
        goto fail;
    }
    pStream = device__pFormatContext->streams[0];
    if(pStream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
        DL_ERROR(1, "stream is not video stream");
        goto fail;
    }

    device__pPacket = av_packet_alloc();
    eState = DFF__EMPTY;

    device__iLastOpener = deviceOpener;
    device__iLastName = deviceName;
    device__iLastFormat = format;
    device__iLastCodec = codecID;
    device__iLastInited = true;

    return initVideoEncoder(format, codecID, pStream->codecpar->width, pStream->codecpar->height, (AVPixelFormat)pStream->codecpar->format);
fail:
    __freeAll();
    return false;
}
bool DFFMpeg::initVideoDecoder(AVPixelFormat preferredFormat) {

    // 1: prepareToInputStream
    // 2: set first data
    // 3: __openStream
    // 4: prepareToDecode


    if( __createReadContext() == false ) {
        DL_FUNCFAIL(1, "__createReadContext");
        return false;
    }

    pFrame = av_frame_alloc();
    pPacket = av_packet_alloc();

    __decode__preferred_format = preferredFormat;

    return true;
}
bool DFFMpeg::initAudioEncoder(AVCodecID codecID, const char *deviceOpener, const char *deviceName) {
    __freeAll();

    AVStream *pStream = nullptr;
    uint64_t ch;
    //--------------------------------------------------------------- device preparing:
    if( (device__pFormatContext = avformat_alloc_context()) == NULL ) {
        DL_FUNCFAIL(1, "avformat_alloc_context");
        goto fail;
    }
    if( (device__pInputFormat = av_find_input_format(deviceOpener)) == nullptr) {
        DL_FUNCFAIL(1, "av_find_input_format");
        goto fail;
    }
    if( (iErrorCode = avformat_open_input(&device__pFormatContext, deviceName, device__pInputFormat, NULL)) < 0 ) {
        DL_FUNCFAIL(1, "avformat_open_input: [%d]", iErrorCode);
        goto fail;
    }
    if( (iErrorCode = avformat_find_stream_info(device__pFormatContext, NULL)) < 0 ) {
        DL_FUNCFAIL(1, "avformat_find_stream_info: [%d]", iErrorCode);
        goto fail;
    }
    if(device__pFormatContext->nb_streams == 0) {
        DL_FUNCFAIL(1, "No streams");
        goto fail;
    }
    if( (pStream = device__pFormatContext->streams[0]) == nullptr ) {
        DL_BADPOINTER(1, "Stream");
        goto fail;
    }
    if(pStream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
        DL_ERROR(1, "No audio stream");
        goto fail;
    }
    //--------------------------------------------------------------- encode preparing:
    if( (ch = av_get_channel_layout("stereo")) == 0 ) {
        DL_FUNCFAIL(1, "av_get_channel_layout");
        goto fail;
    }
    if( (pCodec = avcodec_find_encoder(codecID)) == nullptr ) {
        DL_FUNCFAIL(1, "avcodec_find_encoder");
        goto fail;
    }
    if( (pCodecContext = avcodec_alloc_context3(pCodec)) == nullptr ) {
        DL_FUNCFAIL(1, "avcodec_alloc_context3");
        goto fail;
    }

//    if( (errorCode = avcodec_parameters_to_context(pCodecContext, pStream->codecpar)) < 0 ) {
//        DL_FUNCFAIL(1, "avcodec_parameters_to_context: [%d]", errorCode);
//        return false;
//    }

    pCodecContext->bit_rate = 64000;
    pCodecContext->sample_fmt = AV_SAMPLE_FMT_S16;
    pCodecContext->channel_layout = ch;
    pCodecContext->sample_rate = pStream->codecpar->sample_rate;




    DL_INFO(1, "codec type: [%d] id: [%d] name: [%s] sr: [%d:%d] tb: [%d,%d]",
            pCodecContext->codec_type, pCodecContext->codec_id, avcodec_get_name(pCodecContext->codec_id),
            pCodecContext->sample_rate, pStream->codecpar->sample_rate,
            pCodecContext->time_base.num, pCodecContext->time_base.den
            );

    if ( (iErrorCode = avcodec_open2(pCodecContext, pCodec, NULL)) < 0) {
        DL_FUNCFAIL(1, "avcodec_open2: [%d]", iErrorCode);
        goto fail;
    }

    pFrame = av_frame_alloc();
    pPacket = av_packet_alloc();
    device__pPacket = av_packet_alloc();


    pFrame->format = pCodecContext->sample_fmt;
    pFrame->nb_samples = pCodecContext->frame_size;
    pFrame->channel_layout = pCodecContext->channel_layout;

    if( (iErrorCode = av_frame_get_buffer(pFrame, 0)) < 0 ) {
        DL_FUNCFAIL(1, "av_frame_get_buffer");
        goto fail;
    }


    if( (pFormatContext = avformat_alloc_context()) == NULL ) {
        DL_FUNCFAIL(1, "avformat_alloc_context (2)");
        goto fail;
    }
    __createWriteContext();




    eState = DFF__ENCODER;
    eType = DFF__AUDIO;
    return true;
fail:
    __freeAll();
    return false;
}
bool DFFMpeg::initAudioDecoder() {
    __freeAll();

    if( __createReadContext() == false ) {
        DL_FUNCFAIL(1, "__createReadContext");
        return false;
    }

    pFrame = av_frame_alloc();
    pPacket = av_packet_alloc();

    eType = DFF__AUDIO;

    return true;
}
bool DFFMpeg::initShotEncoder(AVCodecID codecID) {

    if( pCodecContext == nullptr ) {
        DL_BADPOINTER(1, "Codec context");
        return false;
    }
    if( (__shot__pCodec = const_cast<AVCodec*>(avcodec_find_encoder(codecID))) == NULL ) {
        DL_FUNCFAIL(1, "avcodec_find_encoder");
        return false;
    }
    if( (__shot__pCodecContext = avcodec_alloc_context3(pCodec)) == NULL ) {
        DL_FUNCFAIL(1, "avcodec_alloc_context3");
        return false;
    }
    int width = pCodecContext->width;
    int height = pCodecContext->height;

    __shot__pCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    __shot__pCodecContext->width = width;
    __shot__pCodecContext->height = height;

    if( __shot__pCodec->pix_fmts ) {
        if(__encode__raw_format == AV_PIX_FMT_NONE) {
            __shot__pCodecContext->pix_fmt = __shot__pCodec->pix_fmts[0];
        } else  {
            for(int i=0;__shot__pCodec->pix_fmts[i] != AV_PIX_FMT_NONE; ++i) {
                if(__shot__pCodec->pix_fmts[i] == __encode__raw_format) {
                    __shot__pCodecContext->pix_fmt = __encode__raw_format;
                    break;
                }
            }
            if(__shot__pCodecContext->pix_fmt != __encode__raw_format)
                __shot__pCodecContext->pix_fmt = __shot__pCodec->pix_fmts[0];
        }
    } else {
        DL_ERROR(1, "No pix formats for codec");
        return false;
    }
    if(__shot__pCodecContext->pix_fmt != __encode__raw_format) {
        __shot__wScaler.setScaler(width, height, __encode__raw_format, __shot__pCodecContext->pix_fmt);
        if(__shot__wScaler.isReady() == false) {
            DL_ERROR(1, "Can't prepare shot scaler");
            return false;
        }
        __shot__iNeedScaling = true;
    }


    __shot__pFrame = av_frame_alloc();
    __shot__pFrame->width = width;
    __shot__pFrame->height = height;
    __shot__pFrame->format = __shot__pCodecContext->pix_fmt;
    return true;
}
void DFFMpeg::stop() {
    __freeAll();
}
bool DFFMpeg::deviceRestartLast() {

    deviceStop();
    return initVideoEncoder(device__iLastFormat.c_str(), device__iLastCodec, device__iLastOpener.c_str(), device__iLastName.c_str());
}
bool DFFMpeg::deviceStop() {
    __freeAll();
    return true;
}
bool DFFMpeg::devicePause() {

    if( (iErrorCode = av_read_pause(device__pFormatContext)) < 0 ) {
        DL_FUNCFAIL(1, "av_read_pause: [%d]", iErrorCode);
        return false;
    }
    return true;
}
bool DFFMpeg::encode(const uint8_t *rawFrameData) {

    if(isEncoder() == false) {
        DL_ERROR(1, "DFFMpeg no encoder");
        return false;
    }
    if(rawFrameData == nullptr) {
        DL_BADPOINTER(1, "rawFrameData");
        return false;
    }
    if(pCodecContext == nullptr) {
        DL_BADPOINTER(1, "pCodecContext");
        return false;
    }
    if(pFrame == nullptr) {
        DL_BADPOINTER(1, "pFrame");
        return -1;
    }

    int w = pCodecContext->width;
    int h = pCodecContext->height;

    if(w <= 0 || h <= 0) {
        DL_BADVALUE(1, "width: [%d], height: [%d]", w, h);
        return -1;
    }

    iErrorCode = av_image_fill_arrays(
               pFrame->data,
               pFrame->linesize,
               rawFrameData,
                __encode__raw_format,
               w,
               h,
               1);

    if(iErrorCode < 0) {
        DL_FUNCFAIL(1, "av_image_fill_arrays: [%d]", iErrorCode);
        return false;
    }

    return __encodeVideo(pFrame);
}
bool DFFMpeg::encode(AVFrame *frame) {
    if(isEncoder() == false) {
        DL_ERROR(1, "DFFMpeg no encoder");
        return false;
    }
    if(frame == nullptr) {
        DL_BADPOINTER(1, "frame");
        return false;
    }
    if(pCodecContext == nullptr) {
        DL_BADPOINTER(1, "pCodecContext");
        return false;
    }
    if(pFrame == nullptr) {
        DL_BADPOINTER(1, "pFrame");
        return -1;
    }

    if(eType == DFF__VIDEO) {
        int w = pCodecContext->width;
        int h = pCodecContext->height;

        if(w <= 0 || h <= 0) {
            DL_BADVALUE(1, "width: [%d], height: [%d]", w, h);
            return -1;
        }
    }

    return __encodeVideo(frame);
}
bool DFFMpeg::read() {

    if( (iErrorCode = av_read_frame(device__pFormatContext, device__pPacket)) < 0 ) {
        DL_FUNCFAIL(1, "av_read_frame: [%d]", iErrorCode);
        return false;
    }
    return true;
}
bool DFFMpeg::encode() {

    if(eType == DFF__VIDEO) return encode(device__pPacket->data);
    else if (eType == DFF__AUDIO) return __encodeAudio();

    DL_ERROR(1, "No type");
    return false;
}
bool DFFMpeg::encodeImage() {
    if(eType != DFF__VIDEO) {
        return false;
    }

    if(isEncoder() == false) {
        DL_ERROR(1, "DFFMpeg no encoder");
        return false;
    }
    if(device__pPacket->data == nullptr) {
        DL_BADPOINTER(1, "rawFrameData");
        return false;
    }
    if(__shot__pCodecContext == nullptr) {
        DL_BADPOINTER(1, "pCodecContext");
        return false;
    }
    if(__shot__pFrame == nullptr) {
        DL_BADPOINTER(1, "pFrame");
        return -1;
    }

    int w = __shot__pCodecContext->width;
    int h = __shot__pCodecContext->height;

    if(w <= 0 || h <= 0) {
        DL_BADVALUE(1, "width: [%d], height: [%d]", w, h);
        return -1;
    }

    iErrorCode = av_image_fill_arrays(
               __shot__pFrame->data,
               __shot__pFrame->linesize,
               device__pPacket->data,
                __encode__raw_format,
               w,
               h,
               1);

    if(iErrorCode < 0) {
        DL_FUNCFAIL(1, "av_image_fill_arrays: [%d]", iErrorCode);
        return false;
    }

    return __encodeImage(pFrame);
}
timeval DFFMpeg::encodingTime() const {
    return PROFILER::time_dif(&__encode__start_time, &__encode__current_time);
}
int DFFMpeg::encodingSeconds() const {
    return __encode__current_time.tv_sec - __encode__start_time.tv_sec;
}
bool DFFMpeg::setVideoData(const uint8_t *data, size_t size) {
    if(__decode__read_context == nullptr) {
        DL_BADPOINTER(1, "Read Context");
        return false;
    }
    if(data == nullptr) {
        DL_BADPOINTER(1, "data");
        return false;
    }
    if(size == 0) {
        DL_BADVALUE(1, "size");
        return false;
    }
    __decode__read_context->start = data;
    __decode__read_context->end = data + size;

    return true;
}
bool DFFMpeg::decode() {

    if(__decode__is_stream_open == false) {

//        DL_INFO(1, "Open input stream...");
        if( (__decode__is_stream_open = __openInputStream()) == false ) {
            DL_FUNCFAIL(1, "__openInputStream");
            return false;
        }
//        return true;
    }

//    DL_INFO(1, "Read...");
    iErrorCode = 0;
//    while(iErrorCode > -1) {

//        DL_INFO(1, "1: read frame");
        if( (iErrorCode = av_read_frame(pFormatContext, pPacket)) < 0 ) {

//            if(iErrorCode == AVERROR_EOF || iErrorCode == AVERROR(EAGAIN)) break;

            DL_FUNCFAIL(1, "av_read_frame: [%d]", iErrorCode);
            return false;
        }
//        DL_INFO(1, "2: read frame: [%d] ps: [%d]", iErrorCode, pPacket->size);

        if( (iErrorCode = avcodec_send_packet(pCodecContext, pPacket)) < 0 ) {
            DL_FUNCFAIL(1, "avcodec_send_packet: [%d]", iErrorCode);
            return false;
        }
        if( (iErrorCode = avcodec_receive_frame(pCodecContext, pFrame)) < 0 ) {
            DL_FUNCFAIL(1, "avcodec_receive_frame: [%d]", iErrorCode);
            return false;
        }
//        DL_INFO(1, "3: decode frame: [%d]", iErrorCode);

//    }

//    DL_INFO(1, "packet size: [%d] frame: [%d] r: [%d] format: [%d] sr: [%d]",
//            pPacket->size, pFrame->linesize[0], iErrorCode, pFrame->format, pFrame->sample_rate);


    if(eType == DFF__AUDIO) {
        return true;
    }
    if( __decode__decoded_format == AV_PIX_FMT_NONE ) {

        if( (__decode__decoded_format = pCodecContext->pix_fmt) == AV_PIX_FMT_NONE ) {
            DL_ERROR(1, "Bad decoded raw format");
            return false;
        }
        DL_INFO(1, "Define decoded format: [%d]", __decode__decoded_format);

        if(__decode__preferred_format != __decode__decoded_format) {
            iNeedScaling = true;
            wScaler.setScaler(pCodecContext->width, pCodecContext->height, __decode__decoded_format, __decode__preferred_format);
            if(wScaler.isReady() == false) {
                DL_ERROR(1, "Can't prepare scaler");
                return false;
            }
        }
    }


    AVFrame *frame = pFrame;
    if(iNeedScaling) {
        wScaler.scale(pFrame);
        frame = wScaler.scaledFrame();
    }


    size_t reqSize = av_image_get_buffer_size(__decode__preferred_format, pCodecContext->width, pCodecContext->height, 1);
    __decode__complete_data.reformUp(reqSize);

    iErrorCode = av_image_copy_to_buffer(__decode__complete_data.changableData(), reqSize, frame->data, frame->linesize, __decode__preferred_format,
                                        pCodecContext->width, pCodecContext->height, 1);
    if(iErrorCode < 0 ) {
        DL_FUNCFAIL(1, "av_image_copy_to_buffer");
        return false;
    }

    return true;
}
const uint8_t *DFFMpeg::shotData() const {
    return __shot__buffer.constData();
}
int DFFMpeg::shotSize() const {
    return __shot__buffer.size();
}
int DFFMpeg::writtenSize() const {
    if(eType == DFF__VIDEO) {
        if(__encode__write_context == nullptr) {
            DL_BADPOINTER(1, "Write Context");
            return -1;
        }
        return __encode__write_context->bufferCapacity;
    }
    if(eType == DFF__AUDIO) {
        return __encode__audio_buffer.size();
    }
    return -1;
}
const uint8_t *DFFMpeg::writtenData() const {
    if(eType == DFF__VIDEO) {
        if(__encode__write_context == nullptr) {
            DL_BADPOINTER(1, "Write Context");
            return nullptr;
        }
        return __encode__write_context->buffer;
    }
    if(eType == DFF__AUDIO) {
        return __encode__audio_buffer.constData();
    }

    return nullptr;
}
void DFFMpeg::clearWritten() {

    if(eType == DFF__VIDEO) {
        if(__encode__write_context == nullptr) {
            DL_BADPOINTER(1, "Write Context");
            return;
        }
        zero_mem(__encode__write_context->buffer, __encode__write_context->bufferCapacity);
        __encode__write_context->bufferCapacity = 0;
    }
    if(eType == DFF__AUDIO) {
        __encode__audio_buffer.drop();
    }
}
const uint8_t *DFFMpeg::readedData() const {
    return device__pPacket->data;
}
int DFFMpeg::readedSize() const {
    return device__pPacket->size;
}
const uint8_t *DFFMpeg::decodedData() const {
    return __decode__complete_data.constData();
}
int DFFMpeg::decodedSize() const {
    return __decode__complete_data.size();
}
int DFFMpeg::__setOption(const char *key, const char *value, int flags) {
    return av_dict_set(&pOptions, key, value, flags);
}
void DFFMpeg::__clear() {
    eState = DFF__EMPTY;
    eType = DFF__NO_TYPE;
    iErrorCode = 0;
    pFormatContext = nullptr;
    device__iLastInited = false;

    device__pInputFormat = nullptr;
    device__pFormatContext = nullptr;
    device__pPacket = nullptr;
    device__pInputFormat = nullptr;
    device__pFormatContext = nullptr;

    __shot__pCodec = nullptr;
    __shot__pFrame = nullptr;
    __shot__iNeedScaling = false;
    __shot__pCodecContext = nullptr;

    pCodecContext = nullptr;
    pCodec = nullptr;
    pOptions = nullptr;
    pFrame = nullptr;
    pPacket = nullptr;
    pAVIOContext = nullptr;
    pAVIOBuffer = nullptr;
    iAVIOBufferSize = 0;
    pFormat = nullptr;

    iNeedScaling = false;
    __encode__start_time = {0,0};
    __encode__current_time = {0,0};
    __encode__gop_counter = 0;
    __encode__bframes_counter = 0;
    __encode__auto_writing = true;
    __encode__write_context = nullptr;
    __encode__first_frame = true;
    __encode__raw_format = AV_PIX_FMT_NONE;

    __decode__read_context = nullptr;
    __decode__preferred_format = AV_PIX_FMT_NONE;
    __decode__decoded_format = AV_PIX_FMT_NONE;
    __decode__is_stream_open = false;
}
void DFFMpeg::__freeAll() {
    if( device__pFormatContext ) {
        avformat_close_input(&device__pFormatContext);
    }
    if(pFormatContext) {
//        avformat_close_input(&pFormatContext);
            avformat_free_context(pFormatContext);
        pFormatContext = nullptr;
    }
    if(pCodecContext) {
        avcodec_free_context(&pCodecContext);
        pCodecContext = nullptr;
    }
    if(pOptions) {
        av_dict_free(&pOptions);
        pOptions = nullptr;
    }
    if(pFrame) {
        av_frame_free(&pFrame);
        pFrame = nullptr;
    }
    if(pPacket) {
        av_packet_free(&pPacket);
        pPacket = nullptr;
    }

    __clear();
}
bool DFFMpeg::__createWriteContext() {

    if(pFormatContext == nullptr) {
        DL_BADPOINTER(1, "pFormatContext");
        return false;
    }
    iAVIOBufferSize = 1024 * 4;

    auto opaqueWrite = [](void *opaque, uint8_t *buf, int size) -> int {

//        std::cout << "opaqueWrite:"
//                  << " opaque: " << opaque
//                  << " buf: " << (void*)buf
//                  << " size: " << size
//                  << std::endl;


        InnerWriteContext *v = reinterpret_cast<InnerWriteContext*>(opaque);
        int _cap = v->bufferCapacity + size;
        if(_cap > v->bufferSize) {
            if( (v->buffer = reget_mem(v->buffer, _cap)) == NULL ) {
                DL_BADALLOC(1, "_sendBuffer");
                return -1;
            }
            v->bufferSize = _cap;
        }

        copy_mem(v->buffer + v->bufferCapacity, buf, size);
        v->bufferCapacity += size;
        return size;
    };

    if( (__encode__write_context = get_zmem<InnerWriteContext>(1)) == nullptr) {
        DL_BADALLOC(1, "pReadContext");
        goto fail;
    }
    if( (pAVIOBuffer = (uint8_t*)av_malloc(iAVIOBufferSize)) == NULL ) {
        DL_BADALLOC(1, "AVIOBuffer");
        goto fail;
    }
    if( (pAVIOContext = avio_alloc_context
         (
                 pAVIOBuffer,
                 iAVIOBufferSize,
                 1,
                 __encode__write_context,
                 nullptr,
                 opaqueWrite,
                 nullptr
         )
         ) == NULL ) {
        DL_FUNCFAIL(1, "avio_alloc_context");
        goto fail;
    }
    pFormatContext->pb = pAVIOContext;

    return true;
fail:
    __freeWriteContext();
    return false;
}
void DFFMpeg::__freeWriteContext() {
    if(pAVIOContext) {
        av_free(pAVIOContext);
        pAVIOContext = nullptr;
    }
    if(pAVIOBuffer) {
        av_free(pAVIOBuffer);
        pAVIOBuffer = nullptr;
    }
    if(pFormatContext) {
        pFormatContext->pb = nullptr;
    }
    free_mem(__encode__write_context);
}
bool DFFMpeg::__createReadContext() {
    auto opaqueRead = [](void *opaque, uint8_t *buf, int size) -> int {
        InnerReadContext *v = reinterpret_cast<InnerReadContext*>(opaque);
        if(v->start == v->end) {
            return 0;
        }
        int shift = size;
        int dist = std::distance(v->start,v->end);
        if(dist < size) shift = dist;
        memcpy(buf, v->start, shift);
        v->start += shift;
        return shift;
    };
    if(pFormatContext == nullptr) {
        if( (pFormatContext = avformat_alloc_context()) == nullptr) {
            DL_FUNCFAIL(1, "avformat_alloc_context");
            return -1;
        }
    }
    if( (__decode__read_context = get_zmem<InnerReadContext>(1)) == nullptr) {
        DL_BADALLOC(1, "pReadContext");
        goto fail;
    }
    if( (pAVIOBuffer = (uint8_t*)av_malloc(iAVIOBufferSize)) == NULL ) {
        DL_BADALLOC(1, "AVIOBuffer");
        goto fail;
    }
    if( (pAVIOContext = avio_alloc_context
         (
             pAVIOBuffer,
             iAVIOBufferSize,
             0,
             __decode__read_context,
             opaqueRead,
             NULL,
             NULL)
         ) == NULL) {
        DL_FUNCFAIL(1, "avio_alloc_context");
        goto fail;
    }
    pFormatContext->pb = pAVIOContext;

    return true;
fail:
    __freeReadContext();
    return false;
}
void DFFMpeg::__freeReadContext() {

}
bool DFFMpeg::__openInputStream() {

    AVCodecID codecID;
    if( (iErrorCode = avformat_open_input(&pFormatContext, nullptr, nullptr, nullptr )) < 0) {
        DL_FUNCFAIL(1, "avformat_open_input: [%d]", iErrorCode);
        goto fail;
    }
    if(pFormatContext->iformat)
        pFormat = pFormatContext->iformat->name;
//    bIsStreamOpen = true;

    if(pFormatContext == nullptr) {
        DL_BADPOINTER(1, "pFormatContext (Call prepareToInputStream before)");
        goto fail;
    }
    if(pFormatContext->nb_streams <= 0) {
        DL_ERROR(1, "No streams: [%d]", pFormatContext->nb_streams);
        goto fail;
    }
    if(pFormatContext->streams[0] == nullptr) {
        DL_BADPOINTER(1, "AVStream");
        goto fail;
    }
    if(pFormatContext->streams[0]->codecpar == nullptr) {
        DL_BADPOINTER(1, "codecpar");
        goto fail;
    }
    codecID = pFormatContext->streams[0]->codecpar->codec_id;
    if(codecID == AV_CODEC_ID_NONE) {
        DL_BADVALUE(1, "codecID: [%d]", codecID);
        goto fail;
    }
    if( (pCodec = const_cast<AVCodec*>(avcodec_find_decoder(codecID))) == nullptr) {
        DL_FUNCFAIL(1, "avcodec_find_encoder");
        goto fail;
    }
    if( (pCodecContext = avcodec_alloc_context3(pCodec)) == NULL ) {
        DL_FUNCFAIL(1, "avcodec_alloc_context3");
        goto fail;
    }
    if( (iErrorCode = avcodec_parameters_to_context(pCodecContext, pFormatContext->streams[0]->codecpar)) < 0) {
        DL_FUNCFAIL(1, "avcodec_parameters_from_context: [%d]", iErrorCode);
        goto fail;
    }
    if( (iErrorCode = avcodec_open2(pCodecContext, pCodec, nullptr)) < 0 ) {
        DL_FUNCFAIL(1, "avcodec_open2: [%d]", iErrorCode);
        goto fail;
    }

//    DL_INFO(1, "pref format: [%d] fmts list: [%p]", __decode__preferred_format, pCodec->pix_fmts);
    /*
    if(pCodec->pix_fmts) {
        for(int i=0;pCodec->pix_fmts[i] != AV_PIX_FMT_NONE; ++i) {
            DL_INFO(1, ">>> [%d] supported format: [%d]", i, pCodec->pix_fmts[i]);
            if(__decode__preferred_format == pCodec->pix_fmts[i]) {
                pCodecContext->pix_fmt = __decode__preferred_format;
                break;
            }
        }
        if(pCodecContext->pix_fmt != __decode__preferred_format) {
            pCodecContext->pix_fmt = pCodec->pix_fmts[0];
            __needScaling = true;
            scaler.setScaler(pCodecContext->width, pCodecContext->height, pCodecContext->pix_fmt, __decode__preferred_format);
            if(scaler.isReady() == false) {
                DL_ERROR(1, "Can't prepare scaler");
                goto fail;
            }
        }
    }
    */

//    DL_INFO(1, "pref format: [%d] cc format: [%d] cpf: [%d]",
//            __decode__preferred_format, pCodecContext->pix_fmt, pFormatContext->streams[0]->codecpar->format
//            );




    return true;
fail:
    __freeAll();
    return false;
}
int64_t DFFMpeg::__getPts() {

    if( __encode__start_time.tv_sec == 0 ) {
        gettimeofday(&__encode__start_time, NULL);
    }
    gettimeofday(&__encode__current_time, NULL);
    int64_t interval = 0;
    interval = (1000000L * (__encode__current_time.tv_sec - __encode__start_time.tv_sec)) +
            (__encode__current_time.tv_usec - __encode__start_time.tv_usec);

    return av_rescale_q(interval, __encode__longConst, __encode__T);
}
bool DFFMpeg::__encodeVideo(AVFrame *frame) {


    int64_t pts = __getPts();

    if(iNeedScaling) {
        wScaler.scale(frame);
        frame = wScaler.scaledFrame();
    }
    //---------------------------------------
    if (__encode__gop_counter == pCodecContext->gop_size || __encode__gop_counter == 0) {
        // TYPE: I
        pFrame->pict_type = AV_PICTURE_TYPE_I;
        pFrame->key_frame = 1;
        __encode__gop_counter = 0;
    }
    else if(__encode__bframes_counter < pCodecContext->max_b_frames) {
        // TYPE: B
        pFrame->pict_type = AV_PICTURE_TYPE_B;
        pFrame->key_frame = 0;
        ++__encode__bframes_counter;
    }
    else {
        // TYPE: P
        pFrame->pict_type = AV_PICTURE_TYPE_P;
        pFrame->key_frame = 0;
        __encode__bframes_counter = 0;
    }
    ++__encode__gop_counter;
    //---------------------------------------

    frame->pts = pts;
    pFrame->pts = pts;
    pPacket->pts = pts;
    pPacket->dts = pts;

    if( (iErrorCode = avcodec_send_frame(pCodecContext, frame)) < 0 ) {
        DL_FUNCFAIL(1, "avcodec_send_frame: [%d]", iErrorCode);
        return false;
    }
    if( (iErrorCode = avcodec_receive_packet(pCodecContext, pPacket)) < 0 ) {
        DL_FUNCFAIL(1, "avcodec_receive_packet: [%d]", iErrorCode);
        return false;
    }

    pPacket->pts = pts;
    pPacket->dts = pts;

//    DL_INFO(1, "packet encoded: size: [%d] pts: [%d]", pPacket->size, pts);

    if(__encode__auto_writing) {
        if( __writeHeader() == false ) {
            DL_FUNCFAIL(1, "__writeHeader");
            return false;
        }
        __writePacket();
    }
    return true;
}
bool DFFMpeg::__encodeImage(AVFrame *frame) {

    if(__shot__iNeedScaling) {
        __shot__wScaler.scale(frame);
        frame = __shot__wScaler.scaledFrame();
    }
    if( (iErrorCode = avcodec_send_frame(__shot__pCodecContext, frame)) < 0 ) {
        DL_FUNCFAIL(1, "avcodec_send_frame: [%d]", iErrorCode);
        return false;
    }
    if( (iErrorCode = avcodec_receive_packet(__shot__pCodecContext, pPacket)) < 0 ) {
        DL_FUNCFAIL(1, "avcodec_receive_packet: [%d]", iErrorCode);
        return false;
    }
    __shot__buffer.copyData(pPacket->data, pPacket->size);
    return true;
}
bool DFFMpeg::__encodeAudio() {

    if( (iErrorCode = av_frame_make_writable(pFrame)) < 0 ) {
        DL_FUNCFAIL(1, "av_frame_make_writable");
        return false;
    }

    int sourceSamples = device__pPacket->size / 2;
    int frameSize = pFrame->nb_samples * 2;
    int flushed = 0;
    uint16_t *data = reinterpret_cast<uint16_t*>(pFrame->data[0]);
    uint16_t *source = reinterpret_cast<uint16_t*>(device__pPacket->data);

    while(sourceSamples) {
        int flushNow = sourceSamples > frameSize ? frameSize : sourceSamples;
        sourceSamples -= flushNow;
        //----------------------------------------------
        copy_mem(data, source + flushed, flushNow);
        flushed += flushNow;

        if( (iErrorCode = avcodec_send_frame(pCodecContext, pFrame)) < 0 ) {
            DL_FUNCFAIL(1, "avcodec_send_frame: [%d]", iErrorCode);
            return false;
        }
        if( (iErrorCode = avcodec_receive_packet(pCodecContext, pPacket)) < 0 ) {
            DL_FUNCFAIL(1, "avcodec_receive_packet: [%d]", iErrorCode);
            return false;
        }
        if(__encode__auto_writing) {

            __writePacket();
        }
    }

//    if( (errorCode = avcodec_send_frame(pCodecContext, pFrame)) < 0 ) {
//        DL_FUNCFAIL(1, "avcodec_send_frame: [%d]", errorCode);
//        return false;
//    }
//    if( (errorCode = avcodec_receive_packet(pCodecContext, pPacket)) < 0 ) {
//        DL_FUNCFAIL(1, "avcodec_receive_packet: [%d]", errorCode);
//        return false;
//    }

//    DL_INFO(1, "Packet size: [%d] samples: [%d] f: [%d] cl: [%d] b: [%d] flushed: [%d]",
//            read_pPacket->size,
//            pFrame->nb_samples, pFrame->format, pFrame->channel_layout, pFrame->linesize[0],
//            flushed
//            );

//    if(__encode__auto_writing) {

//        __writePacket();
//    }

    return true;
}
bool DFFMpeg::__writeHeader() {

    if(pFormatContext == nullptr || pFormatContext->pb == nullptr) {
        DL_BADPOINTER(1, "pFormatContext: [%p] or pb", pFormatContext);
        return false;
    }
    if(pFormatContext->pb->bytes_written != 0) {
        return true;
    }
    if( (iErrorCode = avformat_write_header(pFormatContext, NULL)) < 0) {
        DL_FUNCFAIL(1, "avformat_write_header: [%d]", iErrorCode);
        return false;
    }


    return true;
}
bool DFFMpeg::__writePacket() {


    if(eType == DFF__VIDEO) {
        if( (iErrorCode = av_write_frame(pFormatContext, pPacket)) < 0) {
            DL_FUNCFAIL(1, "av_write_frame: [%d]", iErrorCode);
            return false;
        }
        if( pFormatContext->oformat == nullptr || pFormatContext->oformat->write_packet == nullptr) {
            DL_BADPOINTER(1, "oformat: [%p] write_packet: [%p]", pFormatContext->oformat, pFormatContext->oformat->write_packet);
            return false;
        }
        if( (iErrorCode = pFormatContext->oformat->write_packet(pFormatContext, NULL)) < 0 ) {
            DL_FUNCFAIL(1, "write_packet: [%d]", iErrorCode);
            return false;
        }
        if(__encode__first_frame) {
            __encode__stream_header.appendData(__encode__write_context->buffer, __encode__write_context->bufferCapacity);
            __encode__first_frame = false;
        }
    }
    if(eType == DFF__AUDIO) {
        __encode__audio_buffer.appendData(pPacket->data, pPacket->size);
    }

    return true;
}
