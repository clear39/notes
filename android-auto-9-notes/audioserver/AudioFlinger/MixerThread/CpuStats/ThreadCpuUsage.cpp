

/**
 * @frameworks/av/include/cpustats/ThreadCpuUsage.h
 * @frameworks/av/media/libcpustats/ThreadCpuUsage.cpp
*/

class ThreadCpuUsage{

}


/**
 * @frameworks/av/include/cpustats/ThreadCpuUsage.h
 * @frameworks/av/media/libcpustats/ThreadCpuUsage.cpp
*/
ThreadCpuUsage::ThreadCpuUsage() :
        mIsEnabled(false),
        mWasEverEnabled(false),
        mAccumulator(0),
        // mPreviousTs
        // mMonotonicTs
        mMonotonicKnown(false)
{
    (void) pthread_once(&sOnceControl, &init);
    /**
     * 
    */
    for (int i = 0; i < sKernelMax; ++i) {
        mCurrentkHz[i] = (uint32_t) ~0;   // unknown
    }
}


/*static*/
void ThreadCpuUsage::init()
{
    // read the number of CPUs
    sKernelMax = 1;
    /**
     * # cat /sys/devices/system/cpu/kernel_max
     * 63
    */
    int fd = open("/sys/devices/system/cpu/kernel_max", O_RDONLY);
    if (fd >= 0) {
#define KERNEL_MAX_SIZE 12
        char kernelMax[KERNEL_MAX_SIZE];
        ssize_t actual = read(fd, kernelMax, sizeof(kernelMax));
        if (actual >= 2 && kernelMax[actual-1] == '\n') {
            sKernelMax = atoi(kernelMax);
            /**
             * frameworks/av/media/libcpustats/include/cpustats/ThreadCpuUsage.h:129:    static const int MAX_CPU = 8;
            */
            if (sKernelMax >= MAX_CPU - 1) {
                ALOGW("kernel_max %d but MAX_CPU %d", sKernelMax, MAX_CPU);
                sKernelMax = MAX_CPU;
            } else if (sKernelMax < 0) {
                ALOGW("kernel_max invalid %d", sKernelMax);
                sKernelMax = 1;
            } else {
                ++sKernelMax;
                ALOGV("number of CPUs %d", sKernelMax);
            }
        } else {
            ALOGW("Can't read number of CPUs");
        }
        (void) close(fd);
    } else {
        ALOGW("Can't open number of CPUs");
    }
    int i;
    for (i = 0; i < MAX_CPU; ++i) {
        sScalingFds[i] = -1;
    }
}


bool ThreadCpuUsage::sampleAndEnable(double& ns)
{
    bool wasEverEnabled = mWasEverEnabled;  // false
    /**
     * enable 调用 setEnabled(true)
    */
    if (enable()) {
        // already enabled, so add a new sample relative to previous
        return sample(ns);

    } else if (wasEverEnabled) {
        // was disabled, but add sample for accumulated time while enabled
        ns = (double) mAccumulator;
        mAccumulator = 0;
        ALOGV("sampleAndEnable %.0f", ns);
        return true;
    } else {
        // first time called
        ns = 0.0;
        ALOGV("sampleAndEnable false");
        return false;
    }
}


bool ThreadCpuUsage::setEnabled(bool isEnabled)
{
    bool wasEnabled = mIsEnabled;
    // only do something if there is a change
    if (isEnabled != wasEnabled) {
        ALOGV("setEnabled(%d)", isEnabled);
        int rc;
        // enabling
        if (isEnabled) {
            rc = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &mPreviousTs);
            if (rc) {
                ALOGE("clock_gettime(CLOCK_THREAD_CPUTIME_ID) errno=%d", errno);
                isEnabled = false;
            } else {
                mWasEverEnabled = true;
                // record wall clock time at first enable
                if (!mMonotonicKnown) {
                    rc = clock_gettime(CLOCK_MONOTONIC, &mMonotonicTs);
                    if (rc) {
                        ALOGE("clock_gettime(CLOCK_MONOTONIC) errno=%d", errno);
                    } else {
                        mMonotonicKnown = true;
                    }
                }
            }
        // disabling
        } else {
            struct timespec ts;
            rc = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
            if (rc) {
                ALOGE("clock_gettime(CLOCK_THREAD_CPUTIME_ID) errno=%d", errno);
            } else {
                long long delta = (ts.tv_sec - mPreviousTs.tv_sec) * 1000000000LL +
                        (ts.tv_nsec - mPreviousTs.tv_nsec);
                mAccumulator += delta;
#if 0
                mPreviousTs = ts;
#endif
            }
        }
        mIsEnabled = isEnabled;
    }
    return wasEnabled;
}



bool ThreadCpuUsage::sample(double &ns)
{
    if (mWasEverEnabled) {
        if (mIsEnabled) {
            struct timespec ts;
            int rc;
            rc = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
            if (rc) {
                ALOGE("clock_gettime(CLOCK_THREAD_CPUTIME_ID) errno=%d", errno);
                ns = 0.0;
                return false;
            } else {
                long long delta = (ts.tv_sec - mPreviousTs.tv_sec) * 1000000000LL +  (ts.tv_nsec - mPreviousTs.tv_nsec);
                mAccumulator += delta;
                mPreviousTs = ts;
            }
        } else {
            mWasEverEnabled = false;
        }
        ns = (double) mAccumulator;
        ALOGV("sample %.0f", ns);
        mAccumulator = 0;
        return true;
    } else {
        ALOGW("Can't add sample because measurements have never been enabled");
        ns = 0.0;
        return false;
    }
}