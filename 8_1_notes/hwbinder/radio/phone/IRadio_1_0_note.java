
//	@out/target/common/gen/JAVA_LIBRARIES/android.hardware.radio-V1.0-java-static_intermediates/android/hardware/radio/V1_0/IRadio.java
public interface IRadio extends android.hidl.base.V1_0.IBase {

	//调用这个函数 ,serviceName = "slot1"
	 public static IRadio getService(String serviceName) throws android.os.RemoteException {
	 	//	@frameworks/base/core/java/android/os/HwBinder.java
        return IRadio.asInterface(android.os.HwBinder.getService("android.hardware.radio@1.0::IRadio",serviceName));
    }

    public static IRadio getService() throws android.os.RemoteException {
    	//	@frameworks/base/core/java/android/os/HwBinder.java
        return IRadio.asInterface(android.os.HwBinder.getService("android.hardware.radio@1.0::IRadio","default"));
    }
}