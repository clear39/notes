//	


//	frameworks/av/media/libaudioclient/include/media/AudioClient.h
class AudioClient {
 public:
    AudioClient() :
        clientUid(-1), clientPid(-1), packageName("") {}

    uid_t clientUid;
    pid_t clientPid;
    String16 packageName;
};


android::AAudioService::AAudioService()
    : BnAAudioService() {
    mAudioClient.clientUid = getuid();   // TODO consider using geteuid()
    mAudioClient.clientPid = getpid();
    mAudioClient.packageName = String16("");
    //
    AAudioClientTracker::getInstance().setAAudioService(this);
}




status_t AAudioService::dump(int fd, const Vector<String16>& args) {
    std::string result;

    if (!dumpAllowed()) {
        std::stringstream ss;
        ss << "Permission denial: can't dump AAudioService from pid=" << IPCThreadState::self()->getCallingPid() << ", uid=" << IPCThreadState::self()->getCallingUid() << "\n";
        result = ss.str();
        ALOGW("%s", result.c_str());
    } else {
        result = mHandleTracker.dump() + AAudioClientTracker::getInstance().dump() + AAudioEndpointManager::getInstance().dump();
    }
    (void)write(fd, result.c_str(), result.size());
    return NO_ERROR;
}
