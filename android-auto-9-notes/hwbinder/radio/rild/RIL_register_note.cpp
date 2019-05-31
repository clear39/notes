//@hardware/ril/libril/ril.cpp:419
extern "C" void
RIL_register (const RIL_RadioFunctions *callbacks) {
    RLOGI("SIM_COUNT: %d", SIM_COUNT);

    if (callbacks == NULL) {
        RLOGE("RIL_register: RIL_RadioFunctions * null");
        return;
    }
    if (callbacks->version < RIL_VERSION_MIN) {
        RLOGE("RIL_register: version %d is to old, min version is %d", callbacks->version, RIL_VERSION_MIN);
        return;
    }

    RLOGE("RIL_register: RIL version %d", callbacks->version);

    if (s_registerCalled > 0) {
        RLOGE("RIL_register has been called more than once. " "Subsequent call ignored");
        return;
    }

    memcpy(&s_callbacks, callbacks, sizeof (RIL_RadioFunctions));

    s_registerCalled = 1;

    RLOGI("s_registerCalled flag set, %d", s_started);
    // Little self-check

    for (int i = 0; i < (int)NUM_ELEMS(s_commands); i++) {
        assert(i == s_commands[i].requestNumber);
    }

    for (int i = 0; i < (int)NUM_ELEMS(s_unsolResponses); i++) {
        assert(i + RIL_UNSOL_RESPONSE_BASE  == s_unsolResponses[i].requestNumber);
    }

    radio::registerService(&s_callbacks, s_commands);//@hardware/ril/libril/ril_service.cpp
    RLOGI("RILHIDL called registerService");

}

//@hardware/ril/include/telephony/ril.h
#ifndef SIM_COUNT
#if defined(ANDROID_SIM_COUNT_2)
#define SIM_COUNT 2
#elif defined(ANDROID_SIM_COUNT_3)
#define SIM_COUNT 3
#elif defined(ANDROID_SIM_COUNT_4)
#define SIM_COUNT 4
#else
#define SIM_COUNT 1
#endif

#ifndef ANDROID_MULTI_SIM
#define SIM_COUNT 1
#endif
#endif


//@hardware/ril/libril/ril_service.cpp
void radio::registerService(RIL_RadioFunctions *callbacks, CommandInfo *commands) {
    using namespace android::hardware;
    int simCount = 1;
    const char *serviceNames[] = {
            android::RIL_getServiceName()   //@hardware/ril/libril/ril.cpp
            #if (SIM_COUNT >= 2)
            , RIL2_SERVICE_NAME
            #if (SIM_COUNT >= 3)
            , RIL3_SERVICE_NAME
            #if (SIM_COUNT >= 4)
            , RIL4_SERVICE_NAME
            #endif
            #endif
            #endif
            };

    #if (SIM_COUNT >= 2)
    simCount = SIM_COUNT;
    #endif

    configureRpcThreadpool(1, true /* callerWillJoin */);//@system/libhidl/transport/HidlTransportSupport.cpp
    for (int i = 0; i < simCount; i++) {
        pthread_rwlock_t *radioServiceRwlockPtr = getRadioServiceRwlock(i);
        int ret = pthread_rwlock_wrlock(radioServiceRwlockPtr);
        assert(ret == 0);

        radioService[i] = new RadioImpl;//  @hardware/ril/libril/ril_service.cpp:
        radioService[i]->mSlotId = i;
        oemHookService[i] = new OemHookImpl;//  @hardware/ril/libril/ril_service.cpp
        oemHookService[i]->mSlotId = i;
        RLOGD("registerService: starting android::hardware::radio::V1_1::IRadio %s",serviceNames[i]);
        android::status_t status = radioService[i]->registerAsService(serviceNames[i]);
        status = oemHookService[i]->registerAsService(serviceNames[i]);

        ret = pthread_rwlock_unlock(radioServiceRwlockPtr);
        assert(ret == 0);
    }

    s_vendorFunctions = callbacks;
    s_commands = commands;
}

