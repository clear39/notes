public class GsmCdmaPhone extends Phone {

    // precisePhoneType 用于标志创建电话为 GSM 还是 CDMA
	public GsmCdmaPhone(Context context, CommandsInterface ci, PhoneNotifier notifier, int phoneId,int precisePhoneType, TelephonyComponentFactory telephonyComponentFactory) {
        this(context, ci, notifier, false, phoneId, precisePhoneType, telephonyComponentFactory);
    }

    public GsmCdmaPhone(Context context, CommandsInterface ci, PhoneNotifier notifier,boolean unitTestMode, int phoneId, int precisePhoneType,TelephonyComponentFactory telephonyComponentFactory) {
        super(precisePhoneType == PhoneConstants.PHONE_TYPE_GSM ? "GSM" : "CDMA",notifier, context, ci, unitTestMode, phoneId, telephonyComponentFactory);

        // phone type needs to be set before other initialization as other objects rely on it
        mPrecisePhoneType = precisePhoneType;
        initOnce(ci);
        initRatSpecific(precisePhoneType);
        // CarrierSignalAgent uses CarrierActionAgent in construction so it needs to be created
        // after CarrierActionAgent.
        mCarrierActionAgent = mTelephonyComponentFactory.makeCarrierActionAgent(this);
        mCarrierSignalAgent = mTelephonyComponentFactory.makeCarrierSignalAgent(this);
        mSST = mTelephonyComponentFactory.makeServiceStateTracker(this, this.mCi);

        
        // DcTracker uses SST so needs to be created after it is instantiated
        mDcTracker = mTelephonyComponentFactory.makeDcTracker(this);

        mSST.registerForNetworkAttached(this, EVENT_REGISTERED_TO_NETWORK, null);

        mDeviceStateMonitor = mTelephonyComponentFactory.makeDeviceStateMonitor(this);

        logd("GsmCdmaPhone: constructor: sub = " + mPhoneId);
    }


}