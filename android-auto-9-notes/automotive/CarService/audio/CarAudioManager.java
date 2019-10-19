
//  @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/packages/services/Car/car-lib/src/android/car/media/CarAudioManager.java


public final class CarAudioManager implements CarManagerBase {

    /**
     * @param groupId The volume group id
     * @return Key to persist volume index for volume group in {@link Settings.Global}
     */
    public static String getVolumeSettingsKeyForGroup(int groupId) {
        // private static final String VOLUME_SETTINGS_KEY_FOR_GROUP_PREFIX = "android.car.VOLUME_GROUP/";
        return VOLUME_SETTINGS_KEY_FOR_GROUP_PREFIX + groupId;
    }

}