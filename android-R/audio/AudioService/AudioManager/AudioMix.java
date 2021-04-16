

// @ frameworks/base/media/java/android/media/audiopolicy/AudioMix.java
public class AudioMix {
	private AudioMixingRule mRule = null;
	private AudioFormat mFormat = null;
	private int mRouteFlags = 0;
	private int mCallbackFlags = 0;
	// an AudioSystem.DEVICE_* value, not AudioDeviceInfo.TYPE_*
	private int mDeviceSystemType = AudioSystem.DEVICE_NONE;
	private String mDeviceAddress = null;
	
	public static class Builder {

	}
}