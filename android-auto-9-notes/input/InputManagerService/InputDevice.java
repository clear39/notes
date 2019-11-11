

//  @   frameworks/base/core/java/android/view/InputDevice.java

public final class InputDevice implements Parcelable {

     /**
     * The input source is a DPad. 一般指键盘上的方向键或手柄上的左方向键(非左摇杆)
     *
     * @see #SOURCE_CLASS_BUTTON
     */
    public static final int SOURCE_DPAD = 0x00000200 | SOURCE_CLASS_BUTTON;

    /**
     * Returns true if the device is a virtual input device rather than a real one,
     * such as the virtual keyboard (see {@link KeyCharacterMap#VIRTUAL_KEYBOARD}).
     * <p>
     * Virtual input devices are provided to implement system-level functionality
     * and should not be seen or configured by users.
     * </p>
     *
     * @return True if the device is virtual.
     *
     * @see KeyCharacterMap#VIRTUAL_KEYBOARD
     */
    public boolean isVirtual() {
        return mId < 0;
    }

    /**
    * Returns true if the device is a full keyboard.
    *
    * @return True if the device is a full keyboard.
    *
    * @hide
    */
   public boolean isFullKeyboard() {
       /***
        * KEYBOARD_TYPE_ALPHABETIC : The keyboard supports a complement of alphabetic keys.

        */
       return (mSources & SOURCE_KEYBOARD) == SOURCE_KEYBOARD && mKeyboardType == KEYBOARD_TYPE_ALPHABETIC;
   }
    
}
