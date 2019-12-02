///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <https://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup av
/// @{


#ifndef SCY_AV_MediaCapture_H
#define SCY_AV_MediaCapture_H

#include "scy/base.h"

#ifdef HAVE_FFMPEG

#include "scy/av/audiodecoder.h"
#include "scy/av/ffmpeg.h"
#include "scy/av/icapture.h"
#include "scy/av/packet.h"
#include "scy/av/videodecoder.h"
#include "scy/interface.h"
#include "scy/packetsignal.h"

#include <deque>
#include <mutex>
#include <condition_variable>
#include <vector>


namespace scy {
namespace av {

using EncodedFrame = std::vector<std::vector<unsigned char>>;
using EncodedFramePtr = std::shared_ptr<EncodedFrame>;


template <typename element_t>
class ThreadSafeQueue
{
private:
    std::deque<element_t> the_queue;
    mutable std::mutex the_mutex;
    std::condition_variable the_condition_variable;

public:
    void push(element_t data)
    {
        {
            std::lock_guard<std::mutex> lock(the_mutex);
            the_queue.push_back(data);
        }

        the_condition_variable.notify_one();
    }

    void pushFront(element_t data)
    {
        {
            std::lock_guard<std::mutex> lock(the_mutex);
            the_queue.push_front(data);
        }

        the_condition_variable.notify_one();
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(the_mutex);
        return the_queue.empty();
    }

    int size() const
    {
        return int(the_queue.size());
    }

    element_t peekNext()
    {
        return the_queue.front();
    }

    void waitForNotEmpty()
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        while(the_queue.empty())
            the_condition_variable.wait(lock);
    }

    element_t waitAndPop()
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        while(the_queue.empty())
        {
            the_condition_variable.wait(lock);
        }

        element_t element =the_queue.front();
        the_queue.pop_front();
        return element;
    }

    void clear()
    {
        while (! the_queue.empty())
            the_queue.pop_front();
    }
};


template <typename T>
struct InputTraits
{
    static bool canBeDropped(const T&) { return true; }
};

template <>
struct InputTraits<EncodedFramePtr>
{
    // don't drop null pointers
    static bool canBeDropped(const EncodedFramePtr& ptr) { return ptr ? true : false; }
};

template <typename T, typename Traits=InputTraits<T>>
class ThreadInput
{
public:
    ThreadInput(bool warnWhenDropping, const std::string &nameForDropWarnings)
        : m_warnWhenDropping(warnWhenDropping)
        , m_nameForDropWarnings(nameForDropWarnings)
    {}

    ~ThreadInput() = default;

    void setDropInputs(bool drop) { m_dropInputs = drop; }
    void setMaxInputsToBuffer(int maxInputs) { m_maxFramesToBuffer = maxInputs; }

    void queueInput(T input)
    {
        m_inputs.push(input);
        int numDropped = 0;
        while (m_inputs.size() > m_maxFramesToBuffer)
        {
            m_inputs.waitAndPop();
            ++numDropped;
        }
        //logWarningIf(m_warnWhenDropping && numDropped > 0,
        //             "{} dropping {} inputs", m_nameForDropWarnings, numDropped);
    }

    T nextInput()
    {
        T input = m_inputs.waitAndPop();

        // drop if requested
        int numDropped = 0;
        while (m_dropInputs && Traits::canBeDropped(input) && ! m_inputs.empty())
        {
            input = m_inputs.waitAndPop();
            ++numDropped;
        }

        if (m_warnWhenDropping && numDropped > 0)
        {
            LDebug(m_nameForDropWarnings, " dropping ", numDropped, " frames");
        }

        return input;
    }

    bool empty() const { return m_inputs.empty(); }

private:
    bool m_dropInputs = true;
    bool m_warnWhenDropping = true;
    std::string m_nameForDropWarnings;
    ThreadSafeQueue<T> m_inputs;
    int m_maxFramesToBuffer = 30;
};


/// This class implements a cross platform audio, video, screen and
/// video file capturer.
class AV_API MediaCapture : public ICapture, public basic::Runnable
{
public:
    typedef std::shared_ptr<MediaCapture> Ptr;

    MediaCapture();
    virtual ~MediaCapture();

    virtual void openFile(const std::string& file, const std::string& format = "");
    // #ifdef HAVE_FFMPEG_AVDEVICE
    // virtual void openCamera(const std::string& device, int width = -1, int height = -1, double framerate = -1);
    // virtual void openMicrophone(const std::string& device, int channels = -1, int sampleRate = -1);
    // #endif

	void openStreamr();
	
	virtual void close();

    virtual void start() override;
    virtual void stop() override;

    virtual void run() override;

    virtual void getEncoderFormat(Format& format);
    virtual void getEncoderAudioCodec(AudioCodec& params);
    virtual void getEncoderVideoCodec(VideoCodec& params);

    /// Continuously loop the input file when set.
    void setLoopInput(bool flag);

    /// Limit playback to video FPS.
    void setLimitFramerate(bool flag);

    /// Set to use realtime PTS calculation.
    /// This is preferred when sing live captures as FFmpeg provided values are
    /// not always reliable.
    void setRealtimePTS(bool flag);

    void onStreamrEncodedFrame(EncodedFramePtr frame);

    AVFormatContext* formatCtx() const;
    VideoDecoder* video() const;
    AudioDecoder* audio() const;
    bool stopping() const;
    std::string error() const;

    /// Signals that the capture thread is closing.
    /// Careful, this signal is emitted from inside the tread contect.
    NullSignal Closing;

protected:
    virtual void openStream(const std::string& filename, AVInputFormat* inputFormat, AVDictionary** formatParams);

    void emit(IPacket& packet);

protected:
    mutable std::mutex _mutex;
    Thread _thread;
    AVFormatContext* _formatCtx;
    VideoDecoder* _video;
    AudioDecoder* _audio;
    std::string _error;
    bool _stopping;
    bool _looping;
    bool _realtime;
    bool _ratelimit;
    bool _captureFromStreamr;
    ThreadInput<EncodedFramePtr> _encodedFrames;
    uint8_t *_fakeFrameBytes = nullptr;
private:
    void runStreamr();
};


} // namespace av
} // namespace scy


#endif
#endif // SCY_AV_MediaCapture_H


/// @\}
