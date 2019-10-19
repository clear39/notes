



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
        /***
         * 
         * 
         * 
         */
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


        /***
         * private final SparseArray<CarAudioDeviceInfo> mCarAudioDeviceInfos = new SparseArray<>();
         * 
         * 这里将 bus 设备地址与 AudioDeviceInfo 添加 mCarAudioDeviceInfos 表中
         */
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
        
        private final SparseIntArray mContextToBus = new SparseIntArray();

        将 ContextNumber 与 busNumber 对应压入 mContextToBus map中 （将 ContextNumber 与 busNumber映射 ）
        检测 AudioControl.cpp 中 配置的对应bus 地址设备是否存在，
        如果 info 为空 直接 弹出警告信息

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
            /**
             * 获取输出设备信息
             */
            AudioFormat mixFormat = new AudioFormat.Builder()
                    .setSampleRate(info.getSampleRate())
                    .setEncoding(info.getEncodingFormat())
                    .setChannelMask(info.getChannelCount())
                    .build();


            /***
             * 
             *  */        
            AudioMixingRule.Builder mixingRuleBuilder = new AudioMixingRule.Builder();
            for (int j = 0; j < mContextToBus.size(); j++) {
                if (mContextToBus.valueAt(j) == busNumber) {
                    hasContext = true;
                    int contextNumber = mContextToBus.keyAt(j);
                    int[] usages = getUsagesForContext(contextNumber);
                    for (int usage : usages) {
                        mixingRuleBuilder.addRule(new AudioAttributes.Builder().setUsage(usage).build(),AudioMixingRule.RULE_MATCH_ATTRIBUTE_USAGE);
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
                        .setFormat(mixFormat)  // 输入设备 采样信息，解码格式，通道数量
                        .setDevice(info.getAudioDeviceInfo())
                        .setRouteFlags(AudioMix.ROUTE_FLAG_RENDER)
                        .build();
                builder.addMix(audioMix);
            }
        }

        // 4th, attach the {@link AudioPolicyVolumeCallback}
        builder.setAudioPolicyVolumeCallback(mAudioPolicyVolumeCallback);
        
        /**
         * 在 build 中通过设置的参数 构造一个 AudioPolicy 并返回
         */
        return builder.build();
    }


    private int[] getUsagesForContext(int contextNumber) {
        final List<Integer> usages = new ArrayList<>();
        for (int i = 0; i < USAGE_TO_CONTEXT.size(); i++) {
            if (USAGE_TO_CONTEXT.valueAt(i) == contextNumber) {
                usages.add(USAGE_TO_CONTEXT.keyAt(i));
            }
        }
        return usages.stream().mapToInt(i -> i).toArray();
    }



    private void setupVolumeGroups() {
        Preconditions.checkArgument(mCarAudioDeviceInfos.size() > 0,"No bus device is configured to setup volume groups");

        /**
        36 <volumeGroups xmlns:car="http://schemas.android.com/apk/res-auto">
        37     <group>
        38     ┊   <context car:context="music"/>
        39     ┊   <context car:context="call_ring"/>
        40     ┊   <context car:context="navigation"/>
        41     ┊   <context car:context="voice_command"/>
        42     ┊   <context car:context="call"/>
        43     </group>
        44     <group>
        45     ┊   <context car:context="notification"/>
        46     ┊   <context car:context="system_sound"/>
        47     ┊   <context car:context="alarm"/>
        48     </group>
        49 </volumeGroups>

         */
        final CarVolumeGroupsHelper helper = new CarVolumeGroupsHelper(mContext, R.xml.car_volume_groups);

        /***
         * 解析 car_volume_groups
         */
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



    private void setMasterMute(boolean mute, int flags) {
        mAudioManager.setMasterMute(mute, flags);

        // When the master mute is turned ON, we want the playing app to get a "pause" command.
        // When the volume is unmuted, we want to resume playback.
        int keycode = mute ? KeyEvent.KEYCODE_MEDIA_PAUSE : KeyEvent.KEYCODE_MEDIA_PLAY;
        mAudioManager.dispatchMediaKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, keycode));
        mAudioManager.dispatchMediaKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, keycode));
    }



}