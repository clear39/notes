//  @/home/xqli/work/workcodes/aosp-p9.x-auto-beta/packages/services/Car/service/src/com/android/car/hal/PowerHalService.java
/**
 * PowerHalService 在 VehicleHal 中构造
 */
public class PowerHalService extends HalServiceBase {

    public PowerHalService(VehicleHal hal) {
        mHal = hal;
    }

    /***
     * 该方法会在 VehicleHal 的 init 方法中调用执行
     */
    public synchronized Collection<VehiclePropConfig> takeSupportedProperties(Collection<VehiclePropConfig> allProperties) {
        for (VehiclePropConfig config : allProperties) {
            switch (config.prop) {
                case AP_POWER_BOOTUP_REASON:
                case AP_POWER_STATE_REQ:
                case AP_POWER_STATE_REPORT:
                case DISPLAY_BRIGHTNESS:
                    mProperties.put(config.prop, config);
                    break;
            }
        }
        return new LinkedList<>(mProperties.values());
    }

    public synchronized void init() {
        /***
         * mProperties 会在 takeSupportedProperties 中 初始化
         */
        for (VehiclePropConfig config : mProperties.values()) {
            if (VehicleHal.isPropertySubscribable(config)) {
                mHal.subscribeProperty(this, config.prop);
            }
        }
        VehiclePropConfig brightnessProperty = mProperties.get(DISPLAY_BRIGHTNESS);
        if (brightnessProperty != null) {
            mMaxDisplayBrightness = brightnessProperty.areaConfigs.size() > 0 ? brightnessProperty.areaConfigs.get(0).maxInt32Value : 0;
            if (mMaxDisplayBrightness <= 0) {
                Log.w(CarLog.TAG_POWER, "Max display brightness from vehicle HAL is invalid:" + mMaxDisplayBrightness);
                mMaxDisplayBrightness = 1;
            }
        }
    }

}