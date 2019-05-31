//  @/home/xqli/work/workcodes/aosp-p9.x-auto-beta/packages/services/Car/service/src/com/android/car/CarService.java
public class CarService extends Service {
    @Override
    public void onCreate() {
        Log.i(CarLog.TAG_SERVICE, "Service onCreate");

        /**
         * 用于错误处理
         */
        mCanBusErrorNotifier = new CanBusErrorNotifier(this /* context */);

        /**
         * 用于　和　android.hardware.automotive.vehicle@2.0-service　交互　代理
         */
        mVehicle = getVehicle();

        if (mVehicle == null) {
            throw new IllegalStateException("Vehicle HAL service is not available.");
        }
        try {
            mVehicleInterfaceName = mVehicle.interfaceDescriptor();
        } catch (RemoteException e) {
            throw new IllegalStateException("Unable to get Vehicle HAL interface descriptor", e);
        }

        Log.i(CarLog.TAG_SERVICE, "Connected to " + mVehicleInterfaceName);

        mICarImpl = new ICarImpl(this,
                mVehicle,
                SystemInterface.Builder.defaultSystemInterface(this).build(),
                mCanBusErrorNotifier,
                mVehicleInterfaceName);
        mICarImpl.init();
        SystemProperties.set("boot.car_service_created", "1");

        /**
         * 
         */
        linkToDeath(mVehicle, mVehicleDeathRecipient);
        ServiceManager.addService("car_service", mICarImpl);
        super.onCreate();
    }
}


//  @/home/xqli/work/workcodes/aosp-p9.x-auto-beta/packages/services/Car/service/src/com/android/car/ICarImpl.java
public class ICarImpl extends ICar.Stub {

    public ICarImpl(Context serviceContext, IVehicle vehicle, SystemInterface systemInterface,
            CanBusErrorNotifier errorNotifier, String vehicleInterfaceName) {
        mContext = serviceContext;
        mSystemInterface = systemInterface;
        mHal = new VehicleHal(vehicle);
        mVehicleInterfaceName = vehicleInterfaceName;
        mSystemActivityMonitoringService = new SystemActivityMonitoringService(serviceContext);
        mCarPowerManagementService = new CarPowerManagementService(mContext, mHal.getPowerHal(),systemInterface);
        mCarPropertyService = new CarPropertyService(serviceContext, mHal.getPropertyHal());
        mCarDrivingStateService = new CarDrivingStateService(serviceContext, mCarPropertyService);
        mCarUXRestrictionsService = new CarUxRestrictionsManagerService(serviceContext,mCarDrivingStateService, mCarPropertyService);
        mCarPackageManagerService = new CarPackageManagerService(serviceContext,mCarUXRestrictionsService,mSystemActivityMonitoringService);
        mCarInputService = new CarInputService(serviceContext, mHal.getInputHal());
        mCarProjectionService = new CarProjectionService(serviceContext, mCarInputService);
        mGarageModeService = new GarageModeService(mContext, mCarPowerManagementService);
        mAppFocusService = new AppFocusService(serviceContext, mSystemActivityMonitoringService);
        mCarAudioService = new CarAudioService(serviceContext);
        mCarNightService = new CarNightService(serviceContext, mCarPropertyService);
        mInstrumentClusterService = new InstrumentClusterService(serviceContext,mAppFocusService, mCarInputService);
        mSystemStateControllerService = new SystemStateControllerService(serviceContext,mCarPowerManagementService, mCarAudioService, this);
        mPerUserCarServiceHelper = new PerUserCarServiceHelper(serviceContext);
        mCarBluetoothService = new CarBluetoothService(serviceContext, mCarPropertyService,mPerUserCarServiceHelper, mCarUXRestrictionsService);
        mVmsSubscriberService = new VmsSubscriberService(serviceContext, mHal.getVmsHal());
        mVmsPublisherService = new VmsPublisherService(serviceContext, mHal.getVmsHal());
        mCarDiagnosticService = new CarDiagnosticService(serviceContext, mHal.getDiagnosticHal());
        mCarStorageMonitoringService = new CarStorageMonitoringService(serviceContext,systemInterface);
        mCarConfigurationService = new CarConfigurationService(serviceContext, new JsonReaderImpl());
        mUserManagerHelper = new CarUserManagerHelper(serviceContext);
        mCarLocationService = new CarLocationService(mContext, mCarPowerManagementService,mCarPropertyService, mUserManagerHelper);

        // Be careful with order. Service depending on other service should be inited later.
        List<CarServiceBase> allServices = new ArrayList<>();
       　。。。。。。

        //通过　getprop 获取  android.car.systemuser.headless　值
        if (mUserManagerHelper.isHeadlessSystemUser()) {
            mCarUserService = new CarUserService(serviceContext, mUserManagerHelper);
            allServices.add(mCarUserService);
        }

        allServices.add(mCarLocationService);

        //  private final CarServiceBase[] mAllServices;
        mAllServices = allServices.toArray(new CarServiceBase[allServices.size()]);
    }

    void init() {
        traceBegin("VehicleHal.init");
        mHal.init();
        traceEnd();
        traceBegin("CarService.initAllServices");
        for (CarServiceBase service : mAllServices) {
            service.init();
        }
        traceEnd();
    }

}


