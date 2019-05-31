//  @/home/xqli/work/workcodes/aosp-p9.x-auto-beta/packages/services/Car/service/src/com/android/car/hal/VehicleHal.java
public class VehicleHal extends IVehicleCallback.Stub {

    public VehicleHal(IVehicle vehicle) {
        mHandlerThread = new HandlerThread("VEHICLE-HAL");
        mHandlerThread.start();
        // passing this should be safe as long as it is just kept and not used in constructor
        mPowerHal = new PowerHalService(this);
        mPropertyHal = new PropertyHalService(this);
        mInputHal = new InputHalService(this);
        mVmsHal = new VmsHalService(this);
        mDiagnosticHal = new DiagnosticHalService(this);
        mAllServices.addAll(Arrays.asList(mPowerHal,
                mInputHal,
                mPropertyHal,
                mDiagnosticHal,
                mVmsHal));

        mHalClient = new HalClient(vehicle, mHandlerThread.getLooper(), this /*IVehicleCallback*/);
    }



    public void init() {
        Set<VehiclePropConfig> properties;
        try {
            /**
             * 获取 android.hardware.automotive.vehicle@2.0-service 中所有属性
             */
            properties = new HashSet<>(mHalClient.getAllPropConfigs());
        } catch (RemoteException e) {
            throw new RuntimeException("Unable to retrieve vehicle property configuration", e);
        }

        synchronized (this) {
            /**
             * 将所有属性添加到 mAllProperties 中 
             */
            // Create map of all properties
            for (VehiclePropConfig p : properties) {
                mAllProperties.put(p.prop, p);
            }
        }

        for (HalServiceBase service: mAllServices) {
            Collection<VehiclePropConfig> taken = service.takeSupportedProperties(properties);
            if (taken == null) {
                continue;
            }
            if (DBG) {
                Log.i(CarLog.TAG_HAL, "HalService " + service + " take properties " + taken.size());
            }
            synchronized (this) {
                for (VehiclePropConfig p: taken) {
                    mPropertyHandlers.append(p.prop, service);
                }
            }
            properties.removeAll(taken);
            service.init();
        }
    }
}