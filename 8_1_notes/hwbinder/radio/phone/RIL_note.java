//	@frameworks/opt/telephony/src/java/com/android/internal/telephony/RIL.java

public final class RIL extends BaseCommands implements CommandsInterface {
	  private IRadio getRadioProxy(Message result) {
        if (!mIsMobileNetworkSupported) {//是否支持mobile,在构造函数中初始化
            if (RILJ_LOGV) riljLog("getRadioProxy: Not calling getService(): wifi-only");
            if (result != null) {
                AsyncResult.forMessage(result, null,CommandException.fromRilErrno(RADIO_NOT_AVAILABLE));
                result.sendToTarget();
            }
            return null;
        }

        if (mRadioProxy != null) {
            return mRadioProxy;
        }

        try {
        	//	 static final String[] HIDL_SERVICE_NAME = {"slot1", "slot2", "slot3"};
        	/*
			out/target/common/gen/JAVA_LIBRARIES/android.hardware.radio-V1.0-java-static_intermediates/android/hardware/radio/V1_0/IRadio.java
			out/target/common/gen/JAVA_LIBRARIES/android.hardware.radio-V1.1-java-static_intermediates/android/hardware/radio/V1_1/IRadio.java
        	*/
            mRadioProxy = IRadio.getService(HIDL_SERVICE_NAME[mPhoneId == null ? 0 : mPhoneId]);
            if (mRadioProxy != null) {
                mRadioProxy.linkToDeath(mRadioProxyDeathRecipient,mRadioProxyCookie.incrementAndGet());
                mRadioProxy.setResponseFunctions(mRadioResponse, mRadioIndication);
            } else {
                riljLoge("getRadioProxy: mRadioProxy == null");
            }
        } catch (RemoteException | RuntimeException e) {
            mRadioProxy = null;
            riljLoge("RadioProxy getService/setResponseFunctions: " + e);
        }

        if (mRadioProxy == null) {
            if (result != null) {
                AsyncResult.forMessage(result, null,CommandException.fromRilErrno(RADIO_NOT_AVAILABLE));
                result.sendToTarget();
            }

            // if service is not up, treat it like death notification to try to get service again
            mRilHandler.sendMessageDelayed(
                    mRilHandler.obtainMessage(EVENT_RADIO_PROXY_DEAD,
                            mRadioProxyCookie.incrementAndGet()),
                    IRADIO_GET_SERVICE_DELAY_MILLIS);
        }

        return mRadioProxy;
    }



    private IOemHook getOemHookProxy(Message result) {
        if (!mIsMobileNetworkSupported) {
            if (RILJ_LOGV) riljLog("getOemHookProxy: Not calling getService(): wifi-only");
            if (result != null) {
                AsyncResult.forMessage(result, null,CommandException.fromRilErrno(RADIO_NOT_AVAILABLE));
                result.sendToTarget();
            }
            return null;
        }

        if (mOemHookProxy != null) {
            return mOemHookProxy;
        }

        try {
            mOemHookProxy = IOemHook.getService(HIDL_SERVICE_NAME[mPhoneId == null ? 0 : mPhoneId]);
            if (mOemHookProxy != null) {
                // not calling linkToDeath() as ril service runs in the same process and death
                // notification for that should be sufficient
                mOemHookProxy.setResponseFunctions(mOemHookResponse, mOemHookIndication);
            } else {
                riljLoge("getOemHookProxy: mOemHookProxy == null");
            }
        } catch (RemoteException | RuntimeException e) {
            mOemHookProxy = null;
            riljLoge("OemHookProxy getService/setResponseFunctions: " + e);
        }

        if (mOemHookProxy == null) {
            if (result != null) {
                AsyncResult.forMessage(result, null,CommandException.fromRilErrno(RADIO_NOT_AVAILABLE));
                result.sendToTarget();
            }

            // if service is not up, treat it like death notification to try to get service again
            mRilHandler.sendMessageDelayed(mRilHandler.obtainMessage(EVENT_RADIO_PROXY_DEAD,mRadioProxyCookie.incrementAndGet()),IRADIO_GET_SERVICE_DELAY_MILLIS);
        }

        return mOemHookProxy;
    }
}


