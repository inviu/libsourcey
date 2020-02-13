///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <https://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup av
/// @{


#include "scy/av/mediacapture.h"

#ifdef HAVE_FFMPEG

#include "scy/av/packet.h"
#include "scy/av/devicemanager.h"
#include "scy/av/fakeVideoDecoder.h"
#include "scy/logger.h"
#include "scy/platform.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}


using std::endl;

namespace webrtc {

// uh-oh, terrible hack ahead.  primitive communication with streamr.


struct ToStreamr
{
    std::function<void()> encoderInit;
    std::function<void()> requestIdrFrame;
    std::function<void(int)> updataVideoBitrate;

    std::function<int()> getMaxVideoFps;
    std::function<int()> getMaxVideoKbps;
    std::function<int()> getVideoWidth;
    std::function<int()> getVideoHeight;
    std::function<bool()> getEnableAutoBitrateAdjustment;
    std::function<int()> getMinIdrIntervalMs;
};

extern ToStreamr& getToStreamr();

} // namespace webrtc

namespace scy {
namespace av {


MediaCapture::MediaCapture()
    : _formatCtx(nullptr)
    , _video(nullptr)
    , _audio(nullptr)
    , _stopping(false)
    , _looping(false)
    , _realtime(false)
    , _ratelimit(false)
    , _captureFromStreamr(false)
    , _encodedFrames(true, "_encodedFrames")
{
    initializeFFmpeg();
}


MediaCapture::~MediaCapture()
{
    close();
    uninitializeFFmpeg();
}


void MediaCapture::close()
{
    LTrace("Closing")

    stop();

    if (_video) {
        delete _video;
        _video = nullptr;
    }

    if (_audio) {
        delete _audio;
        _audio = nullptr;
    }

    if (_formatCtx) {
        avformat_close_input(&_formatCtx);
        _formatCtx = nullptr;
    }

    LTrace("Closing: OK")
}


void MediaCapture::openFile(const std::string& file, const std::string& format)
{
    LTrace("Opening file: ", file)
    AVInputFormat* fmt = nullptr;
    if (!format.empty()) {
        fmt = av_find_input_format(format.c_str());
        if (!fmt) {
            throw std::runtime_error("Unknown input format " + format);
        }
    }

    openStream(file, fmt, nullptr);
}


void MediaCapture::openStream(const std::string& filename, AVInputFormat* inputFormat, AVDictionary** formatParams)
{
    LTrace("Opening stream: ", filename)

    if (_formatCtx)
        throw std::runtime_error("Capture has already been initialized");

    if (avformat_open_input(&_formatCtx, filename.c_str(), inputFormat, formatParams) < 0)
        throw std::runtime_error("Cannot open the media source: " + filename);

    // _formatCtx->max_analyze_duration = 0;
    if (avformat_find_stream_info(_formatCtx, nullptr) < 0)
        throw std::runtime_error("Cannot find stream information: " + filename);

    av_dump_format(_formatCtx, 0, filename.c_str(), 0);

    for (unsigned i = 0; i < _formatCtx->nb_streams; i++) {
        auto stream = _formatCtx->streams[i];
        auto codec = stream->codec;
        if (!_video && codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            _video = new VideoDecoder(stream);
            _video->emitter.attach(packetSlot(this, &MediaCapture::emit));
            _video->create();
            _video->open();
        }
        else if (!_audio && codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            _audio = new AudioDecoder(stream);
            _audio->emitter.attach(packetSlot(this, &MediaCapture::emit));
            _audio->create();
            _audio->open();
        }
    }

    if (!_video && !_audio)
        throw std::runtime_error("Cannot find a valid media stream: " + filename);
}


void MediaCapture::openStreamr()
{
    _captureFromStreamr = true;

    _video = new FakeVideoDecoder(nullptr);
    _video->oparams.width = webrtc::getToStreamr().getVideoWidth();
    _video->oparams.height = webrtc::getToStreamr().getVideoHeight();
    _video->oparams.fps = webrtc::getToStreamr().getMaxVideoFps();
    _video->oparams.pixelFmt = "yuv420p";
}


void MediaCapture::start()
{
    LTrace("Starting")

    std::lock_guard<std::mutex> guard(_mutex);
    assert(_video || _audio || _captureFromStreamr);

    if ((_video || _audio || _captureFromStreamr) && !_thread.running()) {
        LTrace("Initializing thread")
        _stopping = false;
        if (_captureFromStreamr)
            _thread.start(std::bind(&MediaCapture::runStreamr, this));
        else
            _thread.start(std::bind(&MediaCapture::run, this));
    }
}


void MediaCapture::stop()
{
    LTrace("Stopping")

    std::lock_guard<std::mutex> guard(_mutex);

    _stopping = true;
    onStreamrEncodedFrame(EncodedFramePtr());
    if (_thread.running()) {
        LTrace("Terminating thread")
        _thread.cancel();
        _thread.join();
    }
}


void MediaCapture::emit(IPacket& packet)
{
    LTrace("Emit: ", packet.size())

    emitter.emit(packet);
}


void MediaCapture::onStreamrEncodedFrame(EncodedFramePtr frame)
{
    _encodedFrames.queueInput(frame);
}


void MediaCapture::runStreamr()
{
    LTrace("Running capture from streamr loop");

    // create a fake yuv420 frame
    //3840x2160
    //codec_.width = getToStreamr().getVideoWidth();
    //codec_.height = getToStreamr().getVideoHeight();
    const int width = webrtc::getToStreamr().getVideoWidth(), 
        height = webrtc::getToStreamr().getVideoHeight(),
        bytesPerFrame = width * height * 3 / 2;
    _fakeFrameBytes = (uint8_t*)malloc(bytesPerFrame);

    uint8_t *data[4] = { _fakeFrameBytes, data[0] + width*height, data[1] + width*height/4, nullptr };
    int lineSizes[4] = { width, width/2, width/2, 0 };

    VideoCodec format(width, height, webrtc::getToStreamr().getMaxVideoFps());
    int64_t time = 0;

    PlanarVideoPacket videoPacket(data, lineSizes, _video->oparams.pixelFmt, width, height, time);

    try {
        // Looping variables
        int64_t videoPtsOffset = 0;

        // Realtime variables
        int64_t startTime = time::hrtime()/1000;

        // Rate limiting variables
        int64_t lastTimestamp = time::hrtime()/1000;
        // int64_t frameInterval = _video ? fpsToInterval(int(_video->iparams.fps)) : 0;
        // FIXME:  pretend fps is 60 for now
        int64_t frameInterval = fpsToInterval(webrtc::getToStreamr().getMaxVideoFps());

        // Read input packets until complete
        while (EncodedFramePtr frame = _encodedFrames.nextInput())
        {
            STrace << "Read frame encoded frame." << endl;

            if (_stopping)
                break;

            // Realtime PTS calculation in microseconds
            // .. but time::hrtime() returns nanosecon
            int64_t pts = time::hrtime()/1000 - startTime;

            // no idea if these make any sense
            videoPacket.time = pts;
            videoPacket.source = nullptr;
            videoPacket.opaque = nullptr;

            // copy the encoded bitstream into the frame memory.

            // first a magic value and then the size in bytes
            struct Header { uint32_t magic, byteSize; };
            Header *header = (Header*)_fakeFrameBytes;
            header->magic = 0xaabbccdd;

            uint8_t *cursor = _fakeFrameBytes + sizeof(header);
            uint32_t totalBytes = 0;
            for (auto &bytes : *frame)
            {
                totalBytes += (uint32_t)bytes.size();

                memcpy(cursor, bytes.data(), bytes.size());
                cursor += bytes.size();
            }

            header->byteSize = totalBytes;

            emit(videoPacket);
        }
    } catch (std::exception& exc) {
        _error = exc.what();
        LError("Decoder Error: ", _error)
    } catch (...) {
        _error = "Unknown Error";
        LError("Unknown Error")
    }

    if (_stopping || !_looping) {
        LTrace("Exiting")
        _stopping = true;
        Closing.emit();
    }

    free(_fakeFrameBytes);
    _fakeFrameBytes = nullptr;
}


void MediaCapture::run()
{
    LTrace("Running")

    try {
        int res;
        AVPacket ipacket;
        av_init_packet(&ipacket);

        // Looping variables
        int64_t videoPtsOffset = 0;
        int64_t audioPtsOffset = 0;

        // Realtime variables
        int64_t startTime = time::hrtime();

        // Rate limiting variables
        int64_t lastTimestamp = time::hrtime();
        int64_t frameInterval = _video ? fpsToInterval(int(_video->iparams.fps)) : 0;

        // Reset the stream back to the beginning when looping is enabled
        if (_looping) {
            LTrace("Looping")
            for (unsigned i = 0; i < _formatCtx->nb_streams; i++) {
                if (avformat_seek_file(_formatCtx, i, 0, 0, 0, AVSEEK_FLAG_FRAME) < 0) {
                    throw std::runtime_error("Cannot reset media stream");
                }
            }
        }

        // Read input packets until complete
        while ((res = av_read_frame(_formatCtx, &ipacket)) >= 0) {
            STrace << "Read frame: "
                   << "pts=" << ipacket.pts << ", "
                   << "dts=" << ipacket.dts << endl;

            if (_stopping)
                break;

            if (_video && ipacket.stream_index == _video->stream->index) {

                // Realtime PTS calculation in microseconds
                if (_realtime) {
                    ipacket.pts = time::hrtime() - startTime;
                }
                else if (_looping) {
                    // Set the PTS offset when looping
                    if (ipacket.pts == 0 && _video->pts > 0)
                        videoPtsOffset = _video->pts;
                    ipacket.pts += videoPtsOffset;
                }

                // Decode and emit
                if (_video->decode(ipacket)) {
                    STrace << "Decoded video: "
                           << "time=" << _video->time << ", "
                           << "pts=" << _video->pts << endl;
                }

                // Pause the input stream in rate limited mode if the
                // decoder is working too fast
                if (_ratelimit) {
                    auto nsdelay = frameInterval - (time::hrtime() - lastTimestamp);
                    // LDebug("Sleep delay: ", nsdelay, ", ", (time::hrtime() - lastTimestamp), ", ", frameInterval)
                    std::this_thread::sleep_for(std::chrono::nanoseconds(nsdelay));
                    lastTimestamp = time::hrtime();
                }
            }
            else if (_audio && ipacket.stream_index == _audio->stream->index) {

                // Set the PTS offset when looping
                if (_looping) {
                    if (ipacket.pts == 0 && _audio->pts > 0)
                        videoPtsOffset = _audio->pts;
                    ipacket.pts += audioPtsOffset;
                }

                // Decode and emit
                if (_audio->decode(ipacket)) {
                    STrace << "Decoded Audio: "
                           << "time=" << _audio->time << ", "
                           << "pts=" << _audio->pts << endl;
                }
            }

            av_packet_unref(&ipacket);
        }

        // Flush remaining packets
        if (!_stopping && res < 0) {
            if (_video)
                _video->flush();
            if (_audio)
                _audio->flush();
        }

        // End of file or error
        LTrace("Decoder EOF: ", res)
    } catch (std::exception& exc) {
        _error = exc.what();
        LError("Decoder Error: ", _error)
    } catch (...) {
        _error = "Unknown Error";
        LError("Unknown Error")
    }

    if (_stopping || !_looping) {
        LTrace("Exiting")
        _stopping = true;
        Closing.emit();
    }
}


void MediaCapture::getEncoderFormat(Format& format)
{
    format.name = "Capture";
    getEncoderVideoCodec(format.video);
    getEncoderAudioCodec(format.audio);
}


void MediaCapture::getEncoderAudioCodec(AudioCodec& params)
{
    std::lock_guard<std::mutex> guard(_mutex);
    if (_audio) {
        assert(_audio->oparams.channels);
        assert(_audio->oparams.sampleRate);
        assert(!_audio->oparams.sampleFmt.empty());
        params = _audio->oparams;
    }
}


void MediaCapture::getEncoderVideoCodec(VideoCodec& params)
{
    std::lock_guard<std::mutex> guard(_mutex);
    if (_video) {
        assert(_video->oparams.width);
        assert(_video->oparams.height);
        assert(!_video->oparams.pixelFmt.empty());
        params = _video->oparams;
    }
}


AVFormatContext* MediaCapture::formatCtx() const
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _formatCtx;
}


VideoDecoder* MediaCapture::video() const
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _video;
}


AudioDecoder* MediaCapture::audio() const
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _audio;
}


void MediaCapture::setLoopInput(bool flag)
{
    _thread.setRepeating(flag);
    _looping = flag;
}


void MediaCapture::setLimitFramerate(bool flag)
{
    _ratelimit = flag;
}


void MediaCapture::setRealtimePTS(bool flag)
{
    _realtime = flag;
}


bool MediaCapture::stopping() const
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _stopping;
}


std::string MediaCapture::error() const
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _error;
}


} // namespace av
} // namespace scy


#endif


/// @\}
