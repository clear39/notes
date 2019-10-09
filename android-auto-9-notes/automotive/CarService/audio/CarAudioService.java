



//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/packages/services/Car/service/src/com/android/car/CarAudioService.java

public class CarAudioService extends ICarAudio.Stub implements CarServiceBase {
    public CarAudioService(Context context) {
        mContext = context;
        mTelephonyManager = (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        mAudioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
        /**
         * packages/services/Car/service/res/values/config.xml:26:    <bool name="audioUseDynamicRouting">false</bool>
         * device/autolink/imx8q/autolink_8q/overlay_car/packages/services/Car/service/res/values/config.xml:25:    <bool name="audioUseDynamicRouting">true</bool>
         */
        mUseDynamicRouting = mContext.getResources().getBoolean(R.bool.audioUseDynamicRouting);
        /**
         * packages/services/Car/service/res/values/config.xml:29:    <bool name="audioPersistMasterMuteState">true</bool>
         */
        mPersistMasterMuteState = mContext.getResources().getBoolean(R.bool.audioPersistMasterMuteState);
    }


     /**
     * Dynamic routing and volume groups are set only if
     * {@link #mUseDynamicRouting} is {@code true}. Otherwise, this service runs in legacy mode.
     */
    @Override
    public void init() {
        synchronized (mImplLock) {
            if (!mUseDynamicRouting) {// 这里为true
                Log.i(CarLog.TAG_AUDIO, "Audio dynamic routing not configured, run in legacy mode");
                setupLegacyVolumeChangedListener();
            } else {
                setupDynamicRouting();
                setupVolumeGroups();
            }

            // Restore master mute state if applicable
            if (mPersistMasterMuteState) {// 这里为true
                boolean storedMasterMute = Settings.Global.getInt(mContext.getContentResolver(),CarAudioManager.VOLUME_SETTINGS_KEY_MASTER_MUTE, 0) != 0;
                setMasterMute(storedMasterMute, 0);
            }
        }
    }


    private void setupDynamicRouting() {
        final IAudioControl audioControl = getAudioControl();
        if (audioControl == null) {
            return;
        }
        AudioPolicy audioPolicy = getDynamicAudioPolicy(audioControl);
        int r = mAudioManager.registerAudioPolicy(audioPolicy);
        if (r != AudioManager.SUCCESS) {
            throw new RuntimeException("registerAudioPolicy failed " + r);
        }
        mAudioPolicy = audioPolicy;
    }

    @Nullable
    private AudioPolicy getDynamicAudioPolicy(@NonNull IAudioControl audioControl) {
        AudioPolicy.Builder builder = new AudioPolicy.Builder(mContext);
        builder.setLooper(Looper.getMainLooper());

        // 1st, enumerate all output bus device ports
        /***
         * 
         * 
         * 
         */
        AudioDeviceInfo[] deviceInfos = mAudioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
        if (deviceInfos.length == 0) {
            Log.e(CarLog.TAG_AUDIO, "getDynamicAudioPolicy, no output device available, ignore");
            return null;
        }
        for (AudioDeviceInfo info : deviceInfos) {
            Log.v(CarLog.TAG_AUDIO, String.format("output id=%d address=%s type=%s",info.getId(), info.getAddress(), info.getType()));
            if (info.getType() == AudioDeviceInfo.TYPE_BUS) {
                final CarAudioDeviceInfo carInfo = new CarAudioDeviceInfo(info);
                // See also the audio_policy_configuration.xml and getBusForContext in
                // audio control HAL, the bus number should be no less than zero.
                if (carInfo.getBusNumber() >= 0) {
                    mCarAudioDeviceInfos.put(carInfo.getBusNumber(), carInfo);
                    Log.i(CarLog.TAG_AUDIO, "Valid bus found " + carInfo);
                }
            }
        }

        // 2nd, map context to physical bus
        /**
         
        private static final int[] CONTEXT_NUMBERS = new int[] {
            ContextNumber.MUSIC,
            ContextNumber.NAVIGATION,
            ContextNumber.VOICE_COMMAND,
            ContextNumber.CALL_RING,
            ContextNumber.CALL,
            ContextNumber.ALARM,
            ContextNumber.NOTIFICATION,
            ContextNumber.SYSTEM_SOUND
        };
        
         */
        try {
            for (int contextNumber : CONTEXT_NUMBERS) {
                int busNumber = audioControl.getBusForContext(contextNumber);
                mContextToBus.put(contextNumber, busNumber);
                CarAudioDeviceInfo info = mCarAudioDeviceInfos.get(busNumber);
                if (info == null) {
                    Log.w(CarLog.TAG_AUDIO, "No bus configured for context: " + contextNumber);
                }
            }
        } catch (RemoteException e) {
            Log.e(CarLog.TAG_AUDIO, "Error mapping context to physical bus", e);
        }

        // 3rd, enumerate all physical buses and build the routing policy.
        // Note that one can not register audio mix for same bus more than once.
        for (int i = 0; i < mCarAudioDeviceInfos.size(); i++) {
            int busNumber = mCarAudioDeviceInfos.keyAt(i);
            boolean hasContext = false;
            CarAudioDeviceInfo info = mCarAudioDeviceInfos.valueAt(i);
            AudioFormat mixFormat = new AudioFormat.Builder()
                    .setSampleRate(info.getSampleRate())
                    .setEncoding(info.getEncodingFormat())
                    .setChannelMask(info.getChannelCount())
                    .build();
            AudioMixingRule.Builder mixingRuleBuilder = new AudioMixingRule.Builder();
            for (int j = 0; j < mContextToBus.size(); j++) {
                if (mContextToBus.valueAt(j) == busNumber) {
                    hasContext = true;
                    int contextNumber = mContextToBus.keyAt(j);
                    int[] usages = getUsagesForContext(contextNumber);
                    for (int usage : usages) {
                        mixingRuleBuilder.addRule(
                                new AudioAttributes.Builder().setUsage(usage).build(),
                                AudioMixingRule.RULE_MATCH_ATTRIBUTE_USAGE);
                    }
                    Log.i(CarLog.TAG_AUDIO, "Bus number: " + busNumber
                            + " contextNumber: " + contextNumber
                            + " sampleRate: " + info.getSampleRate()
                            + " channels: " + info.getChannelCount()
                            + " usages: " + Arrays.toString(usages));
                }
            }
            if (hasContext) {
                // It's a valid case that an audio output bus is defined in
                // audio_policy_configuration and no context is assigned to it.
                // In such case, do not build a policy mix with zero rules.
                AudioMix audioMix = new AudioMix.Builder(mixingRuleBuilder.build())
                        .setFormat(mixFormat)
                        .setDevice(info.getAudioDeviceInfo())
                        .setRouteFlags(AudioMix.ROUTE_FLAG_RENDER)
                        .build();
                builder.addMix(audioMix);
            }
        }

        // 4th, attach the {@link AudioPolicyVolumeCallback}
        builder.setAudioPolicyVolumeCallback(mAudioPolicyVolumeCallback);

        return builder.build();
    }



    private void setupVolumeGroups() {
        Preconditions.checkArgument(mCarAudioDeviceInfos.size() > 0,"No bus device is configured to setup volume groups");
        final CarVolumeGroupsHelper helper = new CarVolumeGroupsHelper(mContext, R.xml.car_volume_groups);
        mCarVolumeGroups = helper.loadVolumeGroups();
        for (CarVolumeGroup group : mCarVolumeGroups) {
            for (int contextNumber : group.getContexts()) {
                int busNumber = mContextToBus.get(contextNumber);
                group.bind(contextNumber, busNumber, mCarAudioDeviceInfos.get(busNumber));
            }

            // Now that we have all our contexts, ensure the HAL gets our intial value
            group.setCurrentGainIndex(group.getCurrentGainIndex());

            Log.v(CarLog.TAG_AUDIO, "Processed volume group: " + group);
        }
        // Perform validation after all volume groups are processed
        if (!validateVolumeGroups()) {
            throw new RuntimeException("Invalid volume groups configuration");
        }
    }



}