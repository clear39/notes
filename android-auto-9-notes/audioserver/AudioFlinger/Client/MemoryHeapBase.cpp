
//  @   frameworks/native/libs/binder/MemoryHeapBase.cpp
MemoryHeapBase::MemoryHeapBase()
    : mFD(-1), mSize(0), mBase(MAP_FAILED),
      mDevice(NULL), mNeedUnmap(false), mOffset(0)
{
}