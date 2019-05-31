//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/vehicle/2.0/default/common/src/VehicleHalManager.cpp
VehicleEmulator::VehicleEmulator(EmulatedVehicleHalIface* hal,std::unique_ptr<CommBase> comm = CommFactory::create())
        : mHal { hal },mComm(comm.release()),mThread { &VehicleEmulator::rxThread, this} {
    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/vehicle/2.0/default/impl/vhal_v2_0/VehicleEmulator.h
    /**
    *这里是简单赋值操作    
    */
    mHal->registerEmulator(this);
}