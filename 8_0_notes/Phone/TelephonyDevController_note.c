//	TelephonyDevController.create();

/**
 * TelephonyDevController - provides a unified view of the
 * telephony hardware resources on a device.
 *
 * manages the set of HardwareConfig for the framework.
 */
public class TelephonyDevController extends Handler {

   public static TelephonyDevController create() {//构建全局单实例TelephonyDevController
        synchronized (mLock) {
            if (sTelephonyDevController != null) {
                throw new RuntimeException("TelephonyDevController already created!?!");
            }
            sTelephonyDevController = new TelephonyDevController();
            return sTelephonyDevController;
        }
    }

    public static TelephonyDevController getInstance() {//获取全局单实例
        synchronized (mLock) {
            if (sTelephonyDevController == null) {
                throw new RuntimeException("TelephonyDevController not yet created!?!");
            }
            return sTelephonyDevController;
        }
    }


    private TelephonyDevController() {
        initFromResource();

        mModems.trimToSize(); //trimToSize减少ArrayList中没有用到成员
        mSims.trimToSize();
    }


/*
<string-array translatable="false" name="config_telephonyHardware">                                                                                                                                  
  <!-- modem -->
  <item>0,modem,0,0,0,1,1,1</item>
  <!-- sim -->
  <item>1,sim,0,modem</item>
</string-array>
*/
    private void initFromResource() {
        Resources resource = Resources.getSystem();
        String[] hwStrings = resource.getStringArray(com.android.internal.R.array.config_telephonyHardware);
        if (hwStrings != null) {
            for (String hwString : hwStrings) {
                HardwareConfig hw = new HardwareConfig(hwString);//frameworks/opt/telephony/src/java/com/android/internal/telephony/HardwareConfig.java
                if (hw != null) {
                    if (hw.type == HardwareConfig.DEV_HARDWARE_TYPE_MODEM) {
                        updateOrInsert(hw, mModems);
                    } else if (hw.type == HardwareConfig.DEV_HARDWARE_TYPE_SIM) {
                        updateOrInsert(hw, mSims);
                    }
                }
            }
        }
    }

    /**
     * hardware configuration update or insert.
     */
    private static void updateOrInsert(HardwareConfig hw, ArrayList<HardwareConfig> list) {
        int size;
        HardwareConfig item;
        synchronized (mLock) {
            size = list.size();
            for (int i = 0 ; i < size ; i++) {
                item = list.get(i);
                if (item.uuid.compareTo(hw.uuid) == 0) {
                    if (DBG) logd("updateOrInsert: removing: " + item);
                    list.remove(i);
                }
            }
            if (DBG) logd("updateOrInsert: inserting: " + hw);
            list.add(hw);
        }
    }













}
