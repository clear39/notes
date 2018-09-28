public class TelephonyNetworkFactory extends NetworkFactory {

	 public TelephonyNetworkFactory(PhoneSwitcher phoneSwitcher,
            SubscriptionController subscriptionController, SubscriptionMonitor subscriptionMonitor,
            Looper looper, Context context, int phoneId, DcTracker dcTracker) {


        super(looper, context, "TelephonyNetworkFactory[" + phoneId + "]", null);


        mInternalHandler = new InternalHandler(looper);

		/*
		* makeNetworkFilter 
		*/
        setCapabilityFilter(makeNetworkFilter(subscriptionController, phoneId));
        //  private final static int TELEPHONY_NETWORK_SCORE = 50;
        setScoreFilter(TELEPHONY_NETWORK_SCORE);  // 

        mPhoneSwitcher = phoneSwitcher;
        mSubscriptionController = subscriptionController;
        mSubscriptionMonitor = subscriptionMonitor;
        mPhoneId = phoneId;
        LOG_TAG = "TelephonyNetworkFactory[" + phoneId + "]";
        mDcTracker = dcTracker;

        mIsActive = false;
        mPhoneSwitcher.registerForActivePhoneSwitch(mPhoneId, mInternalHandler,EVENT_ACTIVE_PHONE_SWITCH, null);

        mSubscriptionId = INVALID_SUBSCRIPTION_ID;
        mSubscriptionMonitor.registerForSubscriptionChanged(mPhoneId, mInternalHandler,EVENT_SUBSCRIPTION_CHANGED, null);

        mIsDefault = false;
        mSubscriptionMonitor.registerForDefaultDataSubscriptionChanged(mPhoneId, mInternalHandler,EVENT_DEFAULT_SUBSCRIPTION_CHANGED, null);

        register();
    }

    private NetworkCapabilities makeNetworkFilter(SubscriptionController subscriptionController,int phoneId) {
        final int subscriptionId = subscriptionController.getSubIdUsingPhoneId(phoneId);
        return makeNetworkFilter(subscriptionId);
    }

    private NetworkCapabilities makeNetworkFilter(int subscriptionId) {
        NetworkCapabilities nc = new NetworkCapabilities();
        nc.addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR);
        nc.addCapability(NetworkCapabilities.NET_CAPABILITY_MMS);
        nc.addCapability(NetworkCapabilities.NET_CAPABILITY_SUPL);
        nc.addCapability(NetworkCapabilities.NET_CAPABILITY_DUN);
        nc.addCapability(NetworkCapabilities.NET_CAPABILITY_FOTA);
        nc.addCapability(NetworkCapabilities.NET_CAPABILITY_IMS);
        nc.addCapability(NetworkCapabilities.NET_CAPABILITY_CBS);
        nc.addCapability(NetworkCapabilities.NET_CAPABILITY_IA);
        nc.addCapability(NetworkCapabilities.NET_CAPABILITY_RCS);
        nc.addCapability(NetworkCapabilities.NET_CAPABILITY_XCAP);
        nc.addCapability(NetworkCapabilities.NET_CAPABILITY_EIMS);
        nc.addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED);
        nc.addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);
        nc.setNetworkSpecifier(new StringNetworkSpecifier(String.valueOf(subscriptionId)));
        return nc;
    }



    @Override
    public void needNetworkFor(NetworkRequest networkRequest, int score) {
        Message msg = mInternalHandler.obtainMessage(EVENT_NETWORK_REQUEST);
        msg.obj = networkRequest;
        msg.sendToTarget();
    }
    
    //处理 EVENT_NETWORK_REQUEST
    private void onNeedNetworkFor(Message msg) {
        NetworkRequest networkRequest = (NetworkRequest)msg.obj;
        boolean isApplicable = false;
        LocalLog localLog = null;
        if (networkRequest.networkCapabilities.getNetworkSpecifier() == null) {
            // request only for the default network
            localLog = mDefaultRequests.get(networkRequest);
            if (localLog == null) {
                localLog = new LocalLog(REQUEST_LOG_SIZE);
                localLog.log("created for " + networkRequest);
                mDefaultRequests.put(networkRequest, localLog);
                isApplicable = mIsDefault;
            }
        } else {
            localLog = mSpecificRequests.get(networkRequest);
            if (localLog == null) {
                localLog = new LocalLog(REQUEST_LOG_SIZE);
                mSpecificRequests.put(networkRequest, localLog);
                isApplicable = true;
            }
        }
        if (mIsActive && isApplicable) {
            String s = "onNeedNetworkFor";
            localLog.log(s);
            log(s + " " + networkRequest);
            mDcTracker.requestNetwork(networkRequest, localLog);
        } else {
            String s = "not acting - isApp=" + isApplicable + ", isAct=" + mIsActive;
            localLog.log(s);
            log(s + " " + networkRequest);
        }
    }


    // apply or revoke requests if our active-ness changes
    private void onActivePhoneSwitch() {
        final boolean newIsActive = mPhoneSwitcher.isPhoneActive(mPhoneId);
        if (mIsActive != newIsActive) {
            mIsActive = newIsActive;
            String logString = "onActivePhoneSwitch(" + mIsActive + ", " + mIsDefault + ")";
            if (DBG) log(logString);
            if (mIsDefault) {
                applyRequests(mDefaultRequests, (mIsActive ? REQUEST : RELEASE), logString);
            }
            applyRequests(mSpecificRequests, (mIsActive ? REQUEST : RELEASE), logString);
        }
    }

}