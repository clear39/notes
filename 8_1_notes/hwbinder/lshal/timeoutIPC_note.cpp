//  @frameworks/native/cmds/lshal/Timeout.h

class BackgroundTaskState {
public:
    BackgroundTaskState(std::function<void(void)> &&func)
            : mFunc(std::forward<decltype(func)>(func)) {}
    void notify() {
        std::unique_lock<std::mutex> lock(mMutex);
        mFinished = true;
        lock.unlock();
        mCondVar.notify_all();
    }
    template<class C, class D>
    bool wait(std::chrono::time_point<C, D> end) {
        std::unique_lock<std::mutex> lock(mMutex);
        mCondVar.wait_until(lock, end, [this](){ return this->mFinished; });
        return mFinished;
    }
    void operator()() {
        mFunc();
    }
private:
    std::mutex mMutex;
    std::condition_variable mCondVar;
    bool mFinished = false;
    std::function<void(void)> mFunc;
};


void *callAndNotify(void *data) {
    BackgroundTaskState &state = *static_cast<BackgroundTaskState *>(data);
    state();
    state.notify();
    return NULL;
}

template<class R, class P>
bool timeout(std::chrono::duration<R, P> delay, std::function<void(void)> &&func) {
    auto now = std::chrono::system_clock::now();
    BackgroundTaskState state{std::forward<decltype(func)>(func)};
    pthread_t thread;
    if (pthread_create(&thread, NULL, callAndNotify, &state)) {
        std::cerr << "FATAL: could not create background thread." << std::endl;
        return false;
    }
    bool success = state.wait(now + delay);
    if (!success) {
        pthread_kill(thread, SIGINT);
    }
    pthread_join(thread, NULL);
    return success;
}


template<class R, class P, class Function, class I, class... Args>
typename std::result_of<Function(I *, Args...)>::type
timeoutIPC(std::chrono::duration<R, P> wait, const sp<I> &interfaceObject, Function &&func,Args &&... args) {
    using ::android::hardware::Status;
    typename std::result_of<Function(I *, Args...)>::type ret{Status::ok()};
    auto boundFunc = std::bind(std::forward<Function>(func),interfaceObject.get(), std::forward<Args>(args)...);
    bool success = timeout(wait, [&ret, &boundFunc] {
        ret = std::move(boundFunc());
    });
    if (!success) {
        return Status::fromStatusT(TIMED_OUT);
    }
    return ret;
}

template<class Function, class I, class... Args>
typename std::result_of<Function(I *, Args...)>::type
timeoutIPC(const sp<I> &interfaceObject, Function &&func, Args &&... args) {
    //  @frameworks/native/cmds/lshal/Timeout.h:28:static constexpr std::chrono::milliseconds IPC_CALL_WAIT{500};
    return timeoutIPC(IPC_CALL_WAIT, interfaceObject, func, args...);
}
