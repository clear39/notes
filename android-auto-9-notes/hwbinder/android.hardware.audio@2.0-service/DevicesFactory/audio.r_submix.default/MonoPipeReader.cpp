//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/media/libnbaio/MonoPipeReader.cpp
MonoPipeReader::MonoPipeReader(MonoPipe* pipe) :
        NBAIO_Source(pipe->mFormat),
        mPipe(pipe), mFifoReader(mPipe->mFifo, true /*throttlesWriter*/)
{
    ALOGI("%s <init>",__func__);
}


ssize_t MonoPipeReader::availableToRead()
{
    if (CC_UNLIKELY(!mNegotiated)) {
        return NEGOTIATE;
    }
    
    ssize_t ret = mFifoReader.available();
    ALOG_ASSERT(ret <= mPipe->mMaxFrames);

    return ret;
}



ssize_t MonoPipeReader::read(void *buffer, size_t count)
{
    // count == 0 is unlikely and not worth checking for explicitly; will be handled automatically
    ssize_t actual = mFifoReader.read(buffer, count);
    ALOGI("%s %zu %zu %s",__func__,count,actual,(char*) buffer);
    ALOG_ASSERT(actual <= count);
    if (CC_UNLIKELY(actual <= 0)) {
        return actual;
    }
    mFramesRead += (size_t) actual;
    return actual;
}

void MonoPipeReader::onTimestamp(const ExtendedTimestamp &timestamp)
{
    mPipe->mTimestampMutator.push(timestamp);
}
