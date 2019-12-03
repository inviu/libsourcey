#ifndef SCY_AV_FakeVideoDecoder_H
#define SCY_AV_FakeVideoDecoder_H


#include "scy/base.h"

#ifdef HAVE_FFMPEG

#include "scy/av/packet.h"
#include "scy/av/videodecoder.h"


namespace scy {
namespace av {


struct FakeVideoDecoder : public VideoDecoder
{
    FakeVideoDecoder(AVStream* stream);
    virtual ~FakeVideoDecoder();

    virtual void create() override;
    virtual void open() override;
    virtual void close() override;

    /// Decodes a the given input packet.
    /// Input packets should use the raw `AVStream` time base. Time base
    /// conversion will happen internally.
    /// Returns true an output packet was was decoded, false otherwise.
    virtual bool decode(AVPacket& ipacket);

    /// Flushes buffered frames.
    /// This method should be called after decoding
    /// until false is returned.
    virtual void flush();
};


} // namespace av
} // namespace scy


#endif
#endif // SCY_AV_FakeVideoDecoder_H


/// @\}
