//  @   /work/workcodes/aosp-p9.x-auto-ga/packages/services/Car/evs/app/EvsVehicleListener.h
/**
 * hardware/interfaces/automotive/vehicle/2.0/IVehicleCallback.hal
 */
interface IVehicleCallback {

    /**
     * Event callback happens whenever a variable that the API user has
     * subscribed to needs to be reported. This may be based purely on
     * threshold and frequency (a regular subscription, see subscribe call's
     * arguments) or when the IVehicle#set method was called and the actual
     * change needs to be reported.
     *
     * These callbacks are chunked.
     *
     * @param values that has been updated.
     */
    oneway onPropertyEvent(vec<VehiclePropValue> propValues);

    /**
     * This method gets called if the client was subscribed to a property using
     * SubscribeFlags::EVENTS_FROM_ANDROID flag and IVehicle#set(...) method was called.
     *
     * These events must be delivered to subscriber immediately without any
     * batching.
     *
     * @param value Value that was set by a client.
     */
    oneway onPropertySet(VehiclePropValue propValue);

    /**
     * Set property value is usually asynchronous operation. Thus even if
     * client received StatusCode::OK from the IVehicle::set(...) this
     * doesn't guarantee that the value was successfully propagated to the
     * vehicle network. If such rare event occurs this method must be called.
     *
     * @param errorCode - any value from StatusCode enum.
     * @param property - a property where error has happened.
     * @param areaId - bitmask that specifies in which areas the problem has
     *                 occurred, must be 0 for global properties
     */
    oneway onPropertySetError(StatusCode errorCode,
                              int32_t propId,
                              int32_t areaId);
};


class EvsVehicleListener : public IVehicleCallback {

    public:
    // Methods from ::android::hardware::automotive::vehicle::V2_0::IVehicleCallback follow.
    Return<void> onPropertyEvent(const hidl_vec <VehiclePropValue> & /*values*/) override {
        {
            // Our use case is so simple, we don't actually need to update a variable,
            // but the docs seem to say we have to take the lock anyway to keep
            // the condition variable implementation happy.
            std::lock_guard<std::mutex> g(mLock);
        }
        mEventCond.notify_one();
        return Return<void>();
    }

    Return<void> onPropertySet(const VehiclePropValue & /*value*/) override {
        // Ignore the direct set calls (we don't expect to make any anyway)
        return Return<void>();
    }

    Return<void> onPropertySetError(StatusCode      /* errorCode */,
                                    int32_t         /* propId */,
                                    int32_t         /* areaId */) override {
        // We don't set values, so we don't listen for set errors
        return Return<void>();
    }

    bool waitForEvents(int timeout_ms) {
        std::unique_lock<std::mutex> g(mLock);
        std::cv_status result = mEventCond.wait_for(g, std::chrono::milliseconds(timeout_ms));
        return (result == std::cv_status::no_timeout);
    }

    void run(EvsStateControl *pStateController) {
        while (true) {
            // Wait until we have an event to which to react
            // (wake up and validate our current state "just in case" every so often)
            waitForEvents(5000);

            // If we were delivered an event (or it's been a while) update as necessary
            EvsStateControl::Command cmd = {
                .operation = EvsStateControl::Op::CHECK_VEHICLE_STATE,
                .arg1      = 0,
                .arg2      = 0,
            };
            pStateController->postCommand(cmd);
        }
    }

private:
    std::mutex mLock;
    std::condition_variable mEventCond;

}