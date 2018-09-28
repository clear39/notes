public class DcTracker extends Handler {

	public DcTracker(Phone phone) {
        super();
        mPhone = phone;

        if (DBG) log("DCT.constructor");

        mResolver = mPhone.getContext().getContentResolver();
        mUiccController = UiccController.getInstance(); // 这里注意，就算有多个Phone都是共用这个mUiccController
        mUiccController.registerForIccChanged(this, DctConstants.EVENT_ICC_CHANGED, null);
        mAlarmManager = (AlarmManager) mPhone.getContext().getSystemService(Context.ALARM_SERVICE);
        mCm = (ConnectivityManager) mPhone.getContext().getSystemService(Context.CONNECTIVITY_SERVICE);


        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(WifiManager.NETWORK_STATE_CHANGED_ACTION);
        filter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION);
        filter.addAction(INTENT_DATA_STALL_ALARM);
        filter.addAction(INTENT_PROVISIONING_APN_ALARM);
        filter.addAction(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED);
        // TODO - redundent with update call below?
        mDataEnabledSettings.setUserDataEnabled(getDataEnabled());

        mPhone.getContext().registerReceiver(mIntentReceiver, filter, null, mPhone);

        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(mPhone.getContext());
        mAutoAttachOnCreation.set(sp.getBoolean(Phone.DATA_DISABLED_ON_BOOT_KEY, false));

        mSubscriptionManager = SubscriptionManager.from(mPhone.getContext());
        mSubscriptionManager.addOnSubscriptionsChangedListener(mOnSubscriptionsChangedListener);

        HandlerThread dcHandlerThread = new HandlerThread("DcHandlerThread");
        dcHandlerThread.start();
        Handler dcHandler = new Handler(dcHandlerThread.getLooper());
        mDcc = DcController.makeDcc(mPhone, this, dcHandler);
        mDcTesterFailBringUpAll = new DcTesterFailBringUpAll(mPhone, dcHandler);

        mDataConnectionTracker = this;
        registerForAllEvents();
        update();
        mApnObserver = new ApnChangeObserver();
        phone.getContext().getContentResolver().registerContentObserver(Telephony.Carriers.CONTENT_URI, true, mApnObserver);

        initApnContexts();

        for (ApnContext apnContext : mApnContexts.values()) {
            // Register the reconnect and restart actions.
            filter = new IntentFilter();
            filter.addAction(INTENT_RECONNECT_ALARM + '.' + apnContext.getApnType());
            mPhone.getContext().registerReceiver(mIntentReceiver, filter, null, mPhone);
        }

        // Add Emergency APN to APN setting list by default to support EPDN in sim absent cases
        initEmergencyApnSetting();
        addEmergencyApnSetting();

        mProvisionActionName = "com.android.internal.telephony.PROVISION" + phone.getPhoneId();

        mSettingsObserver = new SettingsObserver(mPhone.getContext(), this);
        registerSettingsObserver();
    }


    private void initApnContexts() {
        log("initApnContexts: E");
        // Load device network attributes from resources
        /**	
        		<item>name,type,radio,priority,restoreTime,dependencyMet"</item>   		
            <string-array translatable="false" name="networkAttributes">                                                                                                                                         
			┊   <item>"wifi,1,1,1,-1,true"</item>   
			┊   <item>"mobile,0,0,0,-1,true"</item>
			┊   <item>"mobile_mms,2,0,2,60000,true"</item>
			┊   <item>"mobile_supl,3,0,2,60000,true"</item>
			┊   <item>"mobile_dun,4,0,2,60000,true"</item>
			┊   <item>"mobile_hipri,5,0,3,60000,true"</item>
			┊   <item>"mobile_fota,10,0,2,60000,true"</item>
			┊   <item>"mobile_ims,11,0,2,60000,true"</item>
			┊   <item>"mobile_cbs,12,0,2,60000,true"</item>
			┊   <item>"wifi_p2p,13,1,0,-1,true"</item>
			┊   <item>"mobile_ia,14,0,2,-1,true"</item>
			┊   <item>"mobile_emergency,15,0,2,-1,true"</item>
     		</string-array>
        */
        String[] networkConfigStrings = mPhone.getContext().getResources().getStringArray(com.android.internal.R.array.networkAttributes);


        for (String networkConfigString : networkConfigStrings) {
            NetworkConfig networkConfig = new NetworkConfig(networkConfigString);
            ApnContext apnContext = null;

            switch (networkConfig.type) {
            case ConnectivityManager.TYPE_MOBILE:// 	mobile,0,0,0,-1,true
                apnContext = addApnContext(PhoneConstants.APN_TYPE_DEFAULT, networkConfig);
                break;
            case ConnectivityManager.TYPE_MOBILE_MMS:		//mobile_mms,2,0,2,60000,true
                apnContext = addApnContext(PhoneConstants.APN_TYPE_MMS, networkConfig); //彩信添加  
                break;
            case ConnectivityManager.TYPE_MOBILE_SUPL:		// mobile_supl,3,0,2,60000,true
                apnContext = addApnContext(PhoneConstants.APN_TYPE_SUPL, networkConfig);
                break;
            case ConnectivityManager.TYPE_MOBILE_DUN:		//mobile_dun,4,0,2,60000,true
                apnContext = addApnContext(PhoneConstants.APN_TYPE_DUN, networkConfig);
                break;
            case ConnectivityManager.TYPE_MOBILE_HIPRI:		//mobile_hipri,5,0,3,60000,true
                apnContext = addApnContext(PhoneConstants.APN_TYPE_HIPRI, networkConfig);
                break;
            case ConnectivityManager.TYPE_MOBILE_FOTA:		//mobile_fota,10,0,2,60000,true
                apnContext = addApnContext(PhoneConstants.APN_TYPE_FOTA, networkConfig);
                break;
            case ConnectivityManager.TYPE_MOBILE_IMS:		//mobile_ims,11,0,2,60000,true
                apnContext = addApnContext(PhoneConstants.APN_TYPE_IMS, networkConfig);
                break;
            case ConnectivityManager.TYPE_MOBILE_CBS:		//mobile_cbs,12,0,2,60000,true
                apnContext = addApnContext(PhoneConstants.APN_TYPE_CBS, networkConfig);
                break;
            case ConnectivityManager.TYPE_MOBILE_IA:
                apnContext = addApnContext(PhoneConstants.APN_TYPE_IA, networkConfig);
                break;
            case ConnectivityManager.TYPE_MOBILE_EMERGENCY:	//mobile_emergency,15,0,2,-1,true
                apnContext = addApnContext(PhoneConstants.APN_TYPE_EMERGENCY, networkConfig);
                break;
            default:
                log("initApnContexts: skipping unknown type=" + networkConfig.type);
                continue;
            }
            log("initApnContexts: apnContext=" + apnContext);
        }

        if (VDBG) log("initApnContexts: X mApnContexts=" + mApnContexts);
    }


    private ApnContext addApnContext(String type, NetworkConfig networkConfig) {
        ApnContext apnContext = new ApnContext(mPhone, type, LOG_TAG, networkConfig, this);
        mApnContexts.put(type, apnContext);
        mApnContextsById.put(ApnContext.apnIdForApnName(type), apnContext);
        mPrioritySortedApnContexts.add(apnContext);
        return apnContext;
    }




    public void requestNetwork(NetworkRequest networkRequest, LocalLog log) {
        final int apnId = ApnContext.apnIdForNetworkRequest(networkRequest); //彩信这里返回 DctConstants.APN_MMS_ID
        final ApnContext apnContext = mApnContextsById.get(apnId); //mApnContextsById 在构造函数 initApnContexts() 初始化
        log.log("DcTracker.requestNetwork for " + networkRequest + " found " + apnContext);
        if (apnContext != null){
        	apnContext.requestNetwork(networkRequest, log);
        } 
    }

    //这里由ApnContext回调过来的
    public void setEnabled(int id, boolean enable) {
        Message msg = obtainMessage(DctConstants.EVENT_ENABLE_NEW_APN);
        msg.arg1 = id;
        msg.arg2 = (enable ? DctConstants.ENABLED : DctConstants.DISABLED);
        sendMessage(msg);
    }

    // 处理 DctConstants.EVENT_ENABLE_NEW_APN
    private void onEnableApn(int apnId, int enabled) {
        ApnContext apnContext = mApnContextsById.get(apnId);
        if (apnContext == null) {
            loge("onEnableApn(" + apnId + ", " + enabled + "): NO ApnContext");
            return;
        }
        // TODO change our retry manager to use the appropriate numbers for the new APN
        if (DBG) log("onEnableApn: apnContext=" + apnContext + " call applyNewState");
        applyNewState(apnContext, enabled == DctConstants.ENABLED, apnContext.getDependencyMet());

        if ((enabled == DctConstants.DISABLED) && isOnlySingleDcAllowed(mPhone.getServiceState().getRilDataRadioTechnology()) && !isHigherPriorityApnContextActive(apnContext)) {

            if(DBG) log("onEnableApn: isOnlySingleDcAllowed true & higher priority APN disabled");
            // If the highest priority APN is disabled and only single
            // data call is allowed, try to setup data call on other connectable APN.
            setupDataOnConnectableApns(Phone.REASON_SINGLE_PDN_ARBITRATION);
        }
    }


    private void applyNewState(ApnContext apnContext, boolean enabled == true, boolean met == true) {
        boolean cleanup = false;
        boolean trySetup = false;
        String str ="applyNewState(" + apnContext.getApnType() + ", " + enabled + "(" + apnContext.isEnabled() + "), " + met + "(" + apnContext.getDependencyMet() +"))";
        if (DBG) log(str);
        apnContext.requestLog(str);

        if (apnContext.isReady()) {//第一次执行 这里为 false
            。。。。。。
        } else {
            if (enabled && met) {
                if (apnContext.isEnabled()) { //false
                    。。。。。。
                } else {
                    apnContext.setReason(Phone.REASON_DATA_ENABLED);
                }
                if (apnContext.getState() == DctConstants.State.FAILED) {
                    apnContext.setState(DctConstants.State.IDLE);
                }
                trySetup = true;
            }
        }
        apnContext.setEnabled(enabled); //注意
        apnContext.setDependencyMet(met);//注意
        if (cleanup) cleanUpConnection(true, apnContext);
        if (trySetup) {
            apnContext.resetErrorCodeRetries();
            trySetupData(apnContext);
        }
    }


    private boolean trySetupData(ApnContext apnContext) {

        if (mPhone.getSimulatedRadioControl() != null) {
            // Assume data is connected on the simulator
            // FIXME  this can be improved
            apnContext.setState(DctConstants.State.CONNECTED);
            mPhone.notifyDataConnection(apnContext.getReason(), apnContext.getApnType());

            log("trySetupData: X We're on the simulator; assuming connected retValue=true");
            return true;
        }

        DataConnectionReasons dataConnectionReasons = new DataConnectionReasons();
        boolean isDataAllowed = isDataAllowed(apnContext, dataConnectionReasons);
        String logStr = "trySetupData for APN type " + apnContext.getApnType() + ", reason: " + apnContext.getReason() + ". " + dataConnectionReasons.toString();
        if (DBG) log(logStr);
        apnContext.requestLog(logStr);
        if (isDataAllowed) {
            if (apnContext.getState() == DctConstants.State.FAILED) {
                String str = "trySetupData: make a FAILED ApnContext IDLE so its reusable";
                if (DBG) log(str);
                apnContext.requestLog(str);
                apnContext.setState(DctConstants.State.IDLE);
            }
            int radioTech = mPhone.getServiceState().getRilDataRadioTechnology();
            apnContext.setConcurrentVoiceAndDataAllowed(mPhone.getServiceStateTracker().isConcurrentVoiceAndDataAllowed());
            if (apnContext.getState() == DctConstants.State.IDLE) {
                ArrayList<ApnSetting> waitingApns = buildWaitingApns(apnContext.getApnType(), radioTech);
                if (waitingApns.isEmpty()) {
                    notifyNoData(DcFailCause.MISSING_UNKNOWN_APN, apnContext);
                    notifyOffApnsOfAvailability(apnContext.getReason());
                    String str = "trySetupData: X No APN found retValue=false";
                    if (DBG) log(str);
                    apnContext.requestLog(str);
                    return false;
                } else {
                    apnContext.setWaitingApns(waitingApns);
                    if (DBG) {
                        log ("trySetupData: Create from mAllApnSettings : "  + apnListToString(mAllApnSettings));
                    }
                }
            }

            boolean retValue = setupData(apnContext, radioTech, dataConnectionReasons.contains(DataAllowedReasonType.UNMETERED_APN));
            notifyOffApnsOfAvailability(apnContext.getReason());

            if (DBG) log("trySetupData: X retValue=" + retValue);
            return retValue;
        } else {
            if (!apnContext.getApnType().equals(PhoneConstants.APN_TYPE_DEFAULT) && apnContext.isConnectable()) {
                mPhone.notifyDataConnectionFailed(apnContext.getReason(), apnContext.getApnType());
            }
            notifyOffApnsOfAvailability(apnContext.getReason());

            StringBuilder str = new StringBuilder();

            str.append("trySetupData failed. apnContext = [type=" + apnContext.getApnType()
                    + ", mState=" + apnContext.getState() + ", apnEnabled="
                    + apnContext.isEnabled() + ", mDependencyMet="
                    + apnContext.getDependencyMet() + "] ");

            if (!mDataEnabledSettings.isDataEnabled()) {
                str.append("isDataEnabled() = false. " + mDataEnabledSettings);
            }

            // If this is a data retry, we should set the APN state to FAILED so it won't stay
            // in SCANNING forever.
            if (apnContext.getState() == DctConstants.State.SCANNING) {
                apnContext.setState(DctConstants.State.FAILED);
                str.append(" Stop retrying.");
            }

            if (DBG) log(str.toString());
            apnContext.requestLog(str.toString());
            return false;
        }
    }



    /**
     * Setup a data connection based on given APN type.
     *
     * @param apnContext APN context
     * @param radioTech RAT of the data connection
     * @param unmeteredUseOnly True if this data connection should be only used for unmetered purposes only.
     * @return True if successful, otherwise false.
     */
    private boolean setupData(ApnContext apnContext, int radioTech, boolean unmeteredUseOnly) {
        if (DBG) log("setupData: apnContext=" + apnContext);
        apnContext.requestLog("setupData");
        ApnSetting apnSetting;
        DcAsyncChannel dcac = null;

        apnSetting = apnContext.getNextApnSetting();

        if (apnSetting == null) {
            if (DBG) log("setupData: return for no apn found!");
            return false;
        }

        int profileId = apnSetting.profileId;
        if (profileId == 0) {
            profileId = getApnProfileID(apnContext.getApnType());
        }

        // On CDMA, if we're explicitly asking for DUN, we need have
        // a dun-profiled connection so we can't share an existing one
        // On GSM/LTE we can share existing apn connections provided they support
        // this type.
        if (apnContext.getApnType() != PhoneConstants.APN_TYPE_DUN || teardownForDun() == false) {
            dcac = checkForCompatibleConnectedApnContext(apnContext);
            if (dcac != null) {
                // Get the dcacApnSetting for the connection we want to share.
                ApnSetting dcacApnSetting = dcac.getApnSettingSync();
                if (dcacApnSetting != null) {
                    // Setting is good, so use it.
                    apnSetting = dcacApnSetting;
                }
            }
        }
        if (dcac == null) {
            if (isOnlySingleDcAllowed(radioTech)) {
                if (isHigherPriorityApnContextActive(apnContext)) {
                    if (DBG) {
                        log("setupData: Higher priority ApnContext active.  Ignoring call");
                    }
                    return false;
                }

                // Only lower priority calls left.  Disconnect them all in this single PDP case
                // so that we can bring up the requested higher priority call (once we receive
                // response for deactivate request for the calls we are about to disconnect
                if (cleanUpAllConnections(true, Phone.REASON_SINGLE_PDN_ARBITRATION)) {
                    // If any call actually requested to be disconnected, means we can't
                    // bring up this connection yet as we need to wait for those data calls
                    // to be disconnected.
                    if (DBG) log("setupData: Some calls are disconnecting first.  Wait and retry");
                    return false;
                }

                // No other calls are active, so proceed
                if (DBG) log("setupData: Single pdp. Continue setting up data call.");
            }

            dcac = findFreeDataConnection();

            if (dcac == null) {
                dcac = createDataConnection();
            }

            if (dcac == null) {
                if (DBG) log("setupData: No free DataConnection and couldn't create one, WEIRD");
                return false;
            }
        }
        final int generation = apnContext.incAndGetConnectionGeneration();
        if (DBG) {
            log("setupData: dcac=" + dcac + " apnSetting=" + apnSetting + " gen#=" + generation);
        }

        apnContext.setDataConnectionAc(dcac);
        apnContext.setApnSetting(apnSetting);
        apnContext.setState(DctConstants.State.CONNECTING);
        mPhone.notifyDataConnection(apnContext.getReason(), apnContext.getApnType());

        Message msg = obtainMessage();
        msg.what = DctConstants.EVENT_DATA_SETUP_COMPLETE;
        msg.obj = new Pair<ApnContext, Integer>(apnContext, generation);
        dcac.bringUp(apnContext, profileId, radioTech, unmeteredUseOnly, msg, generation);

        if (DBG) log("setupData: initing!");
        return true;
    }




}