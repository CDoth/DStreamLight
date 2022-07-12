# DStreamLight

DStreamLight using for encode and decode video/audio data and based on ffmpeg.

## DFFMpeg

Base class `DFFMpeg` provide encoding/decoding methods and use ffmpeg library. 

## Getting start

Create `DFFMpeg` instance and init it for your needs:
- For video encoding call `DFFMpeg::initVideoEncoder`
- For audio encoding call `DFFMpeg::initAudioEncoder`
- For video decoding call `DFFMpeg::initVideoDecoder`
- For audio decoding call `DFFMpeg::initAudioDecoder`

After inition `DFFMpeg` instance set inner type and state
```
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
```
## Video encoding

#### Start
To begin encode video first init your `DFFMpeg` instance with `DFFMpeg::initVideoEncoder`. There is 2 ways call this method:

> `bool initVideoEncoder(const char *format, AVCodecID codecID, int width, int height, AVPixelFormat rawFormat = AV_PIX_FMT_NONE)`

Use this way if you want provide raw frames to `DFFMpeg` personally. Another words camera capturing is your job. Or you already has set of frames
and you want feed `DFFMpeg` this frames successively. For this case you need point dimensions of frames and frames raw format. To see list of formats see 
`AVPixelFormat` enum in `ffmpeg` sources or documentation. For most usb cameras it will be `AV_PIX_FMT_YUV420P`.

> `bool initVideoEncoder(const char *format, AVCodecID codecID, const char *deviceOpener, const char *deviceName)`

Use this way if you has camera device and want `DFFMpeg` automatically capture frames from this device before encoding. For this case you need point 
`deviceOpener` - it is tool for video capturing available on your system (for example for Windows - `dshow` and for Linux - `v4l2`) and 
`deviceName` - it is camera name and this text C-string must begin by `"video="` (`"video=CAMERA_NAME"`). 

For both cases you also need point `format`. This is format of video container. For example: `"matroska"` or `"mp4"`.
For both cases you also need point `codecID`. This is codec tag from ffmpeg enumeration `AVCodecID`. For example: `AV_CODEC_ID_H264`.

#### Encode
If you inited your `DFFMpeg` with second way ( automatically capturing device ) use next methods:
- `bool DFFMpeg::read()` - capture raw frame from device.
- `bool DFFMpeg::encode()` - encode raw frame

Check return values of both methods. To use encoded frame data use this:
```
    int writtenSize() const; // size of encoded frame in bytes
    const uint8_t *writtenData() const; // encoded data ( writtenSize bytes )
```
> Keep in mind - `DFFMpeg` accumulate encoded frames in memory. If you call `read` + `encode` 2 times - `DFFMpeg` will contain 2 encoded frames in memory
and `writtenSize` will return common size of this two frames and `writtenData` will return pointer to array wich contain data of two frames. 
If you want get frame by frame using `writtenData` - use method `void clearWritten()` after each frame (`read` + `encode` + `writtenData`).

## Video decoding

#### Start
For decoding video you need special `DFFMpeg` instance ( for encoding and decoding you need different objects ).
Then call `bool initVideoDecoder(AVPixelFormat preferredFormat = AV_PIX_FMT_NONE)` for you `DFFMpeg` instance.

You need point of raw frames you want receive after decoding, for example you want fast render frame and you need rgb format, so use `AV_PIX_FMT_RGB24`

#### Decode
This is two case how you can use `DFFMpeg` for decode (and only one is ready):
- Decoding video from video file from you computer or decoding video file from memory ( not available, in plan )
- Decoding video stream, frame by frame

To decode video stream just take your encoded video data and put it to `DFFMpeg`:
```
  bool DFFMpeg::setVideoData(const uint8_t *data, size_t size);
```
After this call `bool decode()` to decode frame(s) and then use:
```
    const uint8_t * decodedData() const;
    int decodedSize() const;
```
to get decoded frame.

## Audio part

For audio and video you can use same `DFFMpeg` methods.
