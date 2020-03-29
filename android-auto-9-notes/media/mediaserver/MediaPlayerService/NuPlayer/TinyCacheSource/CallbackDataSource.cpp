//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libstagefright/CallbackDataSource.cpp

CallbackDataSource::CallbackDataSource(
    const sp<IDataSource>& binderDataSource)
    : mIDataSource(binderDataSource),
      mIsClosed(false) {
    // Set up the buffer to read into.
    mMemory = mIDataSource->getIMemory();
    mName = String8::format("CallbackDataSource(%d->%d, %s)",getpid(),IPCThreadState::self()->getCallingPid(),mIDataSource->toString().string());

}


status_t CallbackDataSource::initCheck() const {
    if (mMemory == NULL) {
        return UNKNOWN_ERROR;
    }
    return OK;
}


uint32_t CallbackDataSource::flags() {
    return mIDataSource->getFlags();
}