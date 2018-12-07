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