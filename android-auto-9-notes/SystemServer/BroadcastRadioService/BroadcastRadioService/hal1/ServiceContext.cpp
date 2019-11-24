



struct ServiceContext {
    ServiceContext() {}

    std::vector<Module> mModules;

private:
    DISALLOW_COPY_AND_ASSIGN(ServiceContext);
};




struct Module {
    sp<V1_0::IBroadcastRadio> radioModule;
    HalRevision halRev;
    std::vector<hardware::broadcastradio::V1_0::BandConfig> bands;
};