
//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/packages/services/Car/car-lib/src/android/car/ICar.aidl
interface ICar {
    /**
     * IBinder is ICarServiceHelper but passed as IBinder due to aidl hidden.
     * Only this method is oneway as it is called from system server.
     * This should be the 1st method. Do not change the order.
     */
    oneway void setCarServiceHelper(in IBinder helper) = 0;
    IBinder getCarService(in String serviceName) = 1;
    int getCarConnectionType() = 2;
}



//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/packages/services/Car/service/src/com/android/car/ICarImpl.java
public class ICarImpl extends ICar.Stub {

    /***
     * 实现aidl接口，只能有SystemServer调用
     */
    @Override
    public void setCarServiceHelper(IBinder helper) {
        int uid = Binder.getCallingUid();
        if (uid != Process.SYSTEM_UID) {
            throw new SecurityException("Only allowed from system");
        }
        synchronized (this) {
            mICarServiceHelper = ICarServiceHelper.Stub.asInterface(helper);
            mSystemInterface.setCarServiceHelper(mICarServiceHelper);
        }
    }

     /***
     * 实现aidl接口
     */
    @Override
    public IBinder getCarService(String serviceName) {
        switch (serviceName) {
            case Car.AUDIO_SERVICE:
                return mCarAudioService;
            case Car.APP_FOCUS_SERVICE:
                return mAppFocusService;
            case Car.PACKAGE_SERVICE:
                return mCarPackageManagerService;
            case Car.DIAGNOSTIC_SERVICE:
                assertAnyDiagnosticPermission(mContext);
                return mCarDiagnosticService;
            case Car.POWER_SERVICE:
                assertPowerPermission(mContext);
                return mCarPowerManagementService;
            case Car.CABIN_SERVICE:
            case Car.HVAC_SERVICE:
            case Car.INFO_SERVICE:
            case Car.PROPERTY_SERVICE:
            case Car.SENSOR_SERVICE:
            case Car.VENDOR_EXTENSION_SERVICE:
                return mCarPropertyService;
            case Car.CAR_NAVIGATION_SERVICE:
                assertNavigationManagerPermission(mContext);
                IInstrumentClusterNavigation navService = mInstrumentClusterService.getNavigationService();
                return navService == null ? null : navService.asBinder();
            case Car.CAR_INSTRUMENT_CLUSTER_SERVICE:
                assertClusterManagerPermission(mContext);
                return mInstrumentClusterService.getManagerService();
            case Car.PROJECTION_SERVICE:
                assertProjectionPermission(mContext);
                return mCarProjectionService;
            case Car.VMS_SUBSCRIBER_SERVICE:
                assertVmsSubscriberPermission(mContext);
                return mVmsSubscriberService;
            case Car.TEST_SERVICE: {
                assertPermission(mContext, Car.PERMISSION_CAR_TEST_SERVICE);
                synchronized (this) {
                    if (mCarTestService == null) {
                        mCarTestService = new CarTestService(mContext, this);
                    }
                    return mCarTestService;
                }
            }
            case Car.BLUETOOTH_SERVICE:
                return mCarBluetoothService;
            case Car.STORAGE_MONITORING_SERVICE:
                assertPermission(mContext, Car.PERMISSION_STORAGE_MONITORING);
                return mCarStorageMonitoringService;
            case Car.CAR_DRIVING_STATE_SERVICE:
                assertDrivingStatePermission(mContext);
                return mCarDrivingStateService;
            case Car.CAR_UX_RESTRICTION_SERVICE:
                return mCarUXRestrictionsService;
            case Car.CAR_CONFIGURATION_SERVICE:
                return mCarConfigurationService;
            default:
                Log.w(CarLog.TAG_SERVICE, "getCarService for unknown service:" + serviceName);
                return null;
        }
    }

    /***
     * 实现aidl接口
     */
    @Override
    public int getCarConnectionType() {
        return Car.CONNECTION_TYPE_EMBEDDED;
    }


}
