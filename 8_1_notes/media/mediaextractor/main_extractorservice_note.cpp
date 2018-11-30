service mediaextractor /system/bin/mediaextractor
    class main
    user mediaex
    group drmrpc mediadrm
    ioprio rt 4
    writepid /dev/cpuset/foreground/tasks



//	@frameworks/av/services/mediaextractor/main_extractorservice.cpp
static const char kSystemSeccompPolicyPath[] = "/system/etc/seccomp_policy/mediaextractor.policy";
static const char kVendorSeccompPolicyPath[] = "/vendor/etc/seccomp_policy/mediaextractor.policy";

int main(int argc __unused, char** argv)
{
    limitProcessMemory(
        "ro.media.maxmem", /* property that defines limit */
        SIZE_MAX, /* upper limit in bytes */
        20 /* upper limit as percentage of physical RAM */);

    signal(SIGPIPE, SIG_IGN);

    //b/62255959: this forces libutis.so to dlopen vendor version of libutils.so
    //before minijail is on. This is dirty but required since some syscalls such
    //as pread64 are used by linker but aren't allowed in the minijail. By
    //calling the function before entering minijail, we can force dlopen.
    android::report_sysprop_change();

    SetUpMinijail(kSystemSeccompPolicyPath, kVendorSeccompPolicyPath);

    InitializeIcuOrDie();

    strcpy(argv[0], "media.extractor");
    sp<ProcessState> proc(ProcessState::self());
    sp<IServiceManager> sm = defaultServiceManager();
    MediaExtractorService::instantiate();
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
}