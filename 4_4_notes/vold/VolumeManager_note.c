//	if (!(vm = VolumeManager::Instance())) {
VolumeManager *VolumeManager::Instance() {//单实例模式
    if (!sInstance)
        sInstance = new VolumeManager();
    return sInstance;
}


VolumeManager::VolumeManager() {
    mDebug = false;
    mVolumes = new VolumeCollection();	//typedef android::List<Volume *> VolumeCollection;  //Volume 定义在 system/vold/Volume.cpp
    mActiveContainers = new AsecIdCollection(); //typedef android::List<ContainerData*> AsecIdCollection;
    mBroadcaster = NULL;
    mUmsSharingCount = 0;
    mSavedDirtyRatio = -1;
    // set dirty ratio to 0 when UMS is active
    mUmsDirtyRatio = 0;
    mVolManagerDisabled = 0;
}


class ContainerData {
public:
    ContainerData(char* _id, container_type_t _type)
            : id(_id)
            , type(_type)
    {}

    ~ContainerData() {
        if (id != NULL) {
            free(id);
            id = NULL;
        }
    }

    char *id;
    container_type_t type; //typedef enum { ASEC, OBB } container_type_t;
};




//
