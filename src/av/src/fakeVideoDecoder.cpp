#include "scy/av/fakeVideoDecoder.h"

namespace scy {
namespace av {

FakeVideoDecoder::FakeVideoDecoder(AVStream *stream)
    : VideoDecoder(stream)
{

}

FakeVideoDecoder::~FakeVideoDecoder()
{

}

void FakeVideoDecoder::create()
{

}

void FakeVideoDecoder::open()
{

}

void FakeVideoDecoder::close()
{

}

bool scy::av::FakeVideoDecoder::decode(AVPacket &ipacket)
{
    return false;
}

void scy::av::FakeVideoDecoder::flush()
{

}

}
}
