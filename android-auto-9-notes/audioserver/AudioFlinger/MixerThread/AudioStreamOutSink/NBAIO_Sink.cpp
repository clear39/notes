
NBAIO_Port::NBAIO_Port(const NBAIO_Format& format) : mNegotiated(false), mFormat(format),
                                             mFrameSize(Format_frameSize(format)) { }

//  @   frameworks/av/media/libnbaio/NBAIO.cpp
NBAIO_Sink::NBAIO_Sink(const NBAIO_Format& format = Format_Invalid) : NBAIO_Port(format), mFramesWritten(0)
{

}