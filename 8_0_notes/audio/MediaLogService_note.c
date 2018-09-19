//开启MediaLogService，在init.rc中加入export ro.test_harness 1



// MediaLogService 分析

//	frameworks/av/services/medialog/MediaLogService.cpp
MediaLogService::MediaLogService() :
    BnMediaLogService(),
    mMergerShared((NBLog::Shared*) malloc(NBLog::Timeline::sharedSize(kMergeBufferSize))),
    mMerger(mMergerShared, kMergeBufferSize),
    mMergeReader(mMergerShared, kMergeBufferSize, mMerger),
    mMergeThread(new NBLog::MergeThread(mMerger))
{
    mMergeThread->run("MergeThread");
}


void MediaLogService::registerWriter(const sp<IMemory>& shared, size_t size, const char *name)
{
    if (IPCThreadState::self()->getCallingUid() != AID_AUDIOSERVER || shared == 0 ||
            size < kMinSize || size > kMaxSize || name == NULL ||
            shared->size() < NBLog::Timeline::sharedSize(size)) {
        return;
    }
    sp<NBLog::Reader> reader(new NBLog::Reader(shared, size));
    NBLog::NamedReader namedReader(reader, name);
    Mutex::Autolock _l(mLock);
    mNamedReaders.add(namedReader);
    mMerger.addReader(namedReader);
}

void MediaLogService::unregisterWriter(const sp<IMemory>& shared)
{
    if (IPCThreadState::self()->getCallingUid() != AID_AUDIOSERVER || shared == 0) {
        return;
    }
    Mutex::Autolock _l(mLock);
    for (size_t i = 0; i < mNamedReaders.size(); ) {
        if (mNamedReaders[i].reader()->isIMemory(shared)) {
            mNamedReaders.removeAt(i);
        } else {
            i++;
        }
    }
}




// Merger 分析
//	 std::vector<NamedReader> mNamedReaders;


NBLog::Merger::Merger(const void *shared, size_t size):
      mShared((Shared *) shared),	//	Shared * const mShared;
      mFifo(mShared != NULL ? new audio_utils_fifo(size, sizeof(uint8_t), mShared->mBuffer, mShared->mRear, NULL /*throttlesFront*/) : NULL), // std::unique_ptr<audio_utils_fifo> mFifo;
      mFifoWriter(mFifo != NULL ? new audio_utils_fifo_writer(*mFifo) : NULL)	//std::unique_ptr<audio_utils_fifo_writer> mFifoWriter;
{

}

//system/media/audio_utils/include/audio_utils/fifo.h:139



// Merge registered readers, sorted by timestamp
void NBLog::Merger::merge() {
    // FIXME This is called by merge thread
    //       but the access to shared variable mNamedReaders is not yet protected by a lock.
    int nLogs = mNamedReaders.size();
    std::vector<std::unique_ptr<NBLog::Reader::Snapshot>> snapshots(nLogs);
    std::vector<NBLog::EntryIterator> offsets(nLogs);
    for (int i = 0; i < nLogs; ++i) {
        snapshots[i] = mNamedReaders[i].reader()->getSnapshot();
        offsets[i] = snapshots[i]->begin();
    }
    // initialize offsets
    // TODO custom heap implementation could allow to update top, improving performance
    // for bursty buffers
    std::priority_queue<MergeItem, std::vector<MergeItem>, std::greater<MergeItem>> timestamps;
    for (int i = 0; i < nLogs; ++i)
    {
        if (offsets[i] != snapshots[i]->end()) {
            int64_t ts = AbstractEntry::buildEntry(offsets[i])->timestamp();
            timestamps.emplace(ts, i);
        }
    }

    while (!timestamps.empty()) {
        // find minimum timestamp
        int index = timestamps.top().index;
        // copy it to the log, increasing offset
        offsets[index] = AbstractEntry::buildEntry(offsets[index])->copyWithAuthor(mFifoWriter,index);
        // update data structures
        timestamps.pop();
        if (offsets[index] != snapshots[index]->end()) {
            int64_t ts = AbstractEntry::buildEntry(offsets[index])->timestamp();
            timestamps.emplace(ts, index);
        }
    }
}






// MergeThread 分析

// MergeThread is a thread that contains a Merger(合并). It works as a retriggerable(可重触发的) one-shot:
// when triggered(触发的), it awakes for a lapse（失效） of time, during which it periodically（定期地） merges; if
// retriggered, the timeout is reset.
// The thread is triggered on AudioFlinger binder activity.

// merging period when the thread is awake
static const int  kThreadSleepPeriodUs = 1000000 /*1s*/;

bool NBLog::MergeThread::threadLoop() {
    bool doMerge;
    {
        AutoMutex _l(mMutex);
        // If mTimeoutUs is negative(消极的), wait on the condition variable until it's positive.
        // If it's positive, wait kThreadSleepPeriodUs and then merge

        nsecs_t waitTime = mTimeoutUs > 0 ? kThreadSleepPeriodUs * 1000 : LLONG_MAX;
        mCond.waitRelative(mMutex, waitTime);
        doMerge = mTimeoutUs > 0;
        mTimeoutUs -= kThreadSleepPeriodUs;
    }
    if (doMerge) {
        mMerger.merge();
    }
    return true;
}

void NBLog::MergeThread::wakeup() {
    setTimeoutUs(kThreadWakeupPeriodUs);
}

void NBLog::MergeThread::setTimeoutUs(int time) {
    AutoMutex _l(mMutex);
    mTimeoutUs = time;
    mCond.signal();
}

