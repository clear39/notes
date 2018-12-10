//	@frameworks/av/services/mediacodec/android.hardware.media.omx@1.0-service.rc
service mediacodec /vendor/bin/hw/android.hardware.media.omx@1.0-service
    class main
    user mediacodec
    group camera drmrpc mediadrm graphics
    ioprio rt 4
    writepid /dev/cpuset/foreground/tasks







//	@frameworks/av/services/mediacodec/main_codecservice.cpp
static const char kSystemSeccompPolicyPath[] = "/system/etc/seccomp_policy/mediacodec.policy";
static const char kVendorSeccompPolicyPath[] = "/vendor/etc/seccomp_policy/mediacodec.policy";



/**
autolink_8qxp:/ $ cat /vendor/etc/seccomp_policy/mediacodec.policy
mknodat: 1
create: 1
access: 1
mkfifo: 1
fchmodat: 1



autolink_8qxp:/ $ cat /system/etc/seccomp_policy/mediacodec.policy         
# Organized by frequency of systemcall - in descending order for
# best performance.
futex: 1
ioctl: 1
write: 1
prctl: 1
clock_gettime: 1
getpriority: 1
read: 1
close: 1
writev: 1
dup: 1
ppoll: 1
mmap2: 1
getrandom: 1

# mremap: Ensure |flags| are (MREMAP_MAYMOVE | MREMAP_FIXED) TODO: Once minijail
# parser support for '<' is in this needs to be modified to also prevent
# |old_address| and |new_address| from touching the exception vector page, which
# on ARM is statically loaded at 0xffff 0000. See
# http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0211h/Babfeega.html
# for more details.
mremap: arg3 == 3
munmap: 1
mprotect: 1
madvise: 1
openat: 1
sigaltstack: 1
clone: 1
setpriority: 1
getuid32: 1
fstat64: 1
fstatfs64: 1
pread64: 1
faccessat: 1
readlinkat: 1
exit: 1
rt_sigprocmask: 1
set_tid_address: 1
restart_syscall: 1
exit_group: 1
rt_sigreturn: 1
pipe2: 1
gettimeofday: 1
sched_yield: 1
nanosleep: 1
lseek: 1
_llseek: 1
sched_get_priority_max: 1
sched_get_priority_min: 1
statfs64: 1
sched_setscheduler: 1
fstatat64: 1
ugetrlimit: 1
getdents64: 1

# for attaching to debuggerd on process crash
sigaction: 1
tgkill: 1
socket: 1
connect: 1
fcntl64: 1
rt_tgsigqueueinfo: 1
geteuid32: 1
getgid32: 1
getegid32: 1
getgroups32: 1
recvmsg: 1
getpid: 1
gettid: 1

*/



int main(int argc __unused, char** argv)
{
    LOG(INFO) << "mediacodecservice starting";
    bool treble = property_get_bool("persist.media.treble_omx", true);//persist.media.treble_omx没有设置，为true
    if (treble) {
      android::ProcessState::initWithDriver("/dev/vndbinder");
    }

    signal(SIGPIPE, SIG_IGN);
    SetUpMinijail(kSystemSeccompPolicyPath, kVendorSeccompPolicyPath);

    strcpy(argv[0], "media.codec");

    ::android::hardware::configureRpcThreadpool(64, false);
    sp<ProcessState> proc(ProcessState::self());

    if (treble) {
        using namespace ::android::hardware::media::omx::V1_0;
        sp<IOmxStore> omxStore = new implementation::OmxStore();
        if (omxStore == nullptr) {
            LOG(ERROR) << "Cannot create IOmxStore HAL service.";
        } else if (omxStore->registerAsService() != OK) {
            LOG(ERROR) << "Cannot register IOmxStore HAL service.";
        }
        sp<IOmx> omx = new implementation::Omx();
        if (omx == nullptr) {
            LOG(ERROR) << "Cannot create IOmx HAL service.";
        } else if (omx->registerAsService() != OK) {
            LOG(ERROR) << "Cannot register IOmx HAL service.";
        } else {
            LOG(INFO) << "Treble OMX service created.";
        }
    } else {
        MediaCodecService::instantiate();
        LOG(INFO) << "Non-Treble OMX service created.";
    }

    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
}