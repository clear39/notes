//  @/home/xqli/work/workcodes/aosp-p9.x-auto-beta/packages/services/Car/service/src/com/android/car/CarPowerManagementService.java
public class CarPowerManagementService extends ICarPower.Stub implements CarServiceBase,PowerHalService.PowerEventListener {

    public CarPowerManagementService(Context context, PowerHalService powerHal,SystemInterface systemInterface) {
        mContext = context;
        // @/home/xqli/work/workcodes/aosp-p9.x-auto-beta/packages/services/Car/service/src/com/android/car/hal/PowerHalService.java
        mHal = powerHal; 
        mSystemInterface = systemInterface;
    }

    @Override
    public void init() {
        synchronized (this) {
            mHandlerThread = new HandlerThread(CarLog.TAG_POWER);
            mHandlerThread.start();
            mHandler = new PowerHandler(mHandlerThread.getLooper());
        }

        mHal.setListener(this);
        if (mHal.isPowerStateSupported()) {
            mHal.sendBootComplete();
            PowerState currentState = mHal.getCurrentPowerState();
            if (currentState != null) {
                onApPowerStateChange(currentState);
            } else {
                Log.w(CarLog.TAG_POWER, "Unable to get get current power state during "
                        + "initialization");
            }
        } else {
            Log.w(CarLog.TAG_POWER, "Vehicle hal does not support power state yet.");
            onApPowerStateChange(new PowerState(PowerHalService.STATE_ON_FULL, 0));
            mSystemInterface.switchToFullWakeLock();
        }
        mSystemInterface.startDisplayStateMonitoring(this);
    }


}