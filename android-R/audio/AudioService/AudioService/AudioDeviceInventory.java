/**
* frameworks/base/services/core/java/com/android/server/audio/AudioDeviceBroker.java:118:        
* mDeviceInventory = new AudioDeviceInventory(this);
*/
public class AudioDeviceInventory {

 /*package*/ AudioDeviceInventory(@NonNull AudioDeviceBroker broker) {
        mDeviceBroker = broker;
        mAudioSystem = AudioSystemAdapter.getDefaultAdapter();
    }
    
}