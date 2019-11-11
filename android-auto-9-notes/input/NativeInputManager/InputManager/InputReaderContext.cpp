

//  @   frameworks/native/services/inputflinger/InputReader.h
class ContextImpl : public InputReaderContext {
    InputReader* mReader;

public:
    explicit ContextImpl(InputReader* reader);

    virtual void updateGlobalMetaState();
    virtual int32_t getGlobalMetaState();
    virtual void disableVirtualKeysUntil(nsecs_t time);
    virtual bool shouldDropVirtualKey(nsecs_t now,
            InputDevice* device, int32_t keyCode, int32_t scanCode);
    virtual void fadePointer();
    virtual void requestTimeoutAtTime(nsecs_t when);
    virtual int32_t bumpGeneration();
    virtual void getExternalStylusDevices(Vector<InputDeviceInfo>& outDevices);
    virtual void dispatchExternalStylusState(const StylusState& outState);
    virtual InputReaderPolicyInterface* getPolicy();
    virtual InputListenerInterface* getListener();
    virtual EventHubInterface* getEventHub();
} mContext;


InputReader::ContextImpl::ContextImpl(InputReader* reader) :
        mReader(reader) {
}