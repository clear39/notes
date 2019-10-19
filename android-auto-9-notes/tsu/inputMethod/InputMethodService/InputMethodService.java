

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/core/java/android/inputmethodservice/InputMethodService.java
public class InputMethodService extends AbstractInputMethodService {



    /**
     * Implement to return our standard {@link InputMethodImpl}.  Subclasses
     * can override to provide their own customized version.
     */
    @Override
    public AbstractInputMethodImpl onCreateInputMethodInterface() {
        return new InputMethodImpl();
    }



    /**
     * A utility method to call {{@link #onShowInputRequested(int, boolean)}} and update internal
     * states depending on its result.  Since {@link #onShowInputRequested(int, boolean)} is
     * exposed to IME authors as an overridable public method without {@code @CallSuper}, we have
     * to have this method to ensure that those internal states are always updated no matter how
     * {@link #onShowInputRequested(int, boolean)} is overridden by the IME author.
     * @param flags Provides additional information about the show request,
     * as per {@link InputMethod#showSoftInput InputMethod.showSoftInput()}.
     * @param configChange This is true if we are re-showing due to a
     * configuration change.
     * @return Returns true to indicate that the window should be shown.
     * @see #onShowInputRequested(int, boolean)
     */
    private boolean dispatchOnShowInputRequested(int flags, boolean configChange) {
        final boolean result = onShowInputRequested(flags, configChange);
        if (DEBUG) Log.v(TAG, "dispatchOnShowInputRequested: flags=" + HexDump.toHexString(flags) + " configChange:"+configChange
                + " result:" +result);
        if (result) {
            mShowInputFlags = flags;
        } else {
            mShowInputFlags = 0;
        }
        return result;
    }


    /**
     * The system has decided that it may be time to show your input method.
     * This is called due to a corresponding call to your
     * {@link InputMethod#showSoftInput InputMethod.showSoftInput()}
     * method.  The default implementation uses
     * {@link #onEvaluateInputViewShown()}, {@link #onEvaluateFullscreenMode()},
     * and the current configuration to decide whether the input view should
     * be shown at this point.
     * 
     * @param flags Provides additional information about the show request,
     * as per {@link InputMethod#showSoftInput InputMethod.showSoftInput()}.
     * @param configChange This is true if we are re-showing due to a
     * configuration change.
     * @return Returns true to indicate that the window should be shown.
     */
    public boolean onShowInputRequested(int flags, boolean configChange) {
        if (!onEvaluateInputViewShown()) {
            if (DEBUG) Log.v(TAG, "111 return false");
            return false;
        }
        if ((flags&InputMethod.SHOW_EXPLICIT) == 0) {
            if (!configChange && onEvaluateFullscreenMode()) {
                // Don't show if this is not explicitly requested by the user and
                // the input method is fullscreen.  That would be too disruptive.
                // However, we skip this change for a config change, since if
                // the IME is already shown we do want to go into fullscreen
                // mode at this point.
                if (DEBUG) Log.v(TAG, "333 return false");
                return false;
            }
            if (!mSettingsObserver.shouldShowImeWithHardKeyboard() &&
                    getResources().getConfiguration().keyboard != Configuration.KEYBOARD_NOKEYS) {
                // And if the device has a hard keyboard, even if it is
                // currently hidden, don't show the input method implicitly.
                // These kinds of devices don't need it that much.
                if (DEBUG) Log.v(TAG, "44444 return false");
                return false;
            }
        }
        return true;
    }


    /**
     * Override this to control when the soft input area should be shown to the user.  The default
     * implementation returns {@code false} when there is no hard keyboard or the keyboard is hidden
     * unless the user shows an intention to use software keyboard.  If you change what this
     * returns, you will need to call {@link #updateInputViewShown()} yourself whenever the returned
     * value may have changed to have it re-evaluated and applied.
     *
     * <p>When you override this method, it is recommended to call
     * {@code super.onEvaluateInputViewShown()} and return {@code true} when {@code true} is
     * returned.</p>
     */
    @CallSuper
    public boolean onEvaluateInputViewShown() {
        if (mSettingsObserver == null) {
            Log.w(TAG, "onEvaluateInputViewShown: mSettingsObserver must not be null here.");
            return false;
        }
        if (mSettingsObserver.shouldShowImeWithHardKeyboard()) {
            return true;
        }
        Configuration config = getResources().getConfiguration();

        /**
         * 
         * 正常的：
         * Configuration={1.0 460mcc4mnc [zh_CN] ldltr sw720dp w1920dp h610dp 160dpi xlrg long land car night finger -keyb/v/h -nav/h winConfig={ mBounds=Rect(0, 0 - 0, 0) mAppBounds=Rect(0, 0 - 1920, 720) mWindowingMode=fullscreen mActivityType=undefined} s.8}
         * finger -keyb/v/h -nav/h
         * 
         * 
         * 不正常的：
         * Configuration={1.0 460mcc4mnc [zh_CN] ldltr sw480dp w800dp h407dp 160dpi lrg long land car night -touch qwerty/v/v dpad/v winConfig={ mBounds=Rect(0, 0 - 0, 0) mAppBounds=Rect(0, 0 - 800, 480) mWindowingM}
         * -touch qwerty/v/v dpad/v
         * 
         * 
         * Configuration
         *  
         switch (touchscreen) {  //
            case TOUCHSCREEN_UNDEFINED: sb.append(" ?touch"); break;
            case TOUCHSCREEN_NOTOUCH: sb.append(" -touch"); break;
            case TOUCHSCREEN_STYLUS: sb.append(" stylus"); break;
            case TOUCHSCREEN_FINGER: sb.append(" finger"); break;
            default: sb.append(" touch="); sb.append(touchscreen); break;
        }
        switch (keyboard) { //
            case KEYBOARD_UNDEFINED: sb.append(" ?keyb"); break;
            case KEYBOARD_NOKEYS: sb.append(" -keyb"); break;
            case KEYBOARD_QWERTY: sb.append(" qwerty"); break;
            case KEYBOARD_12KEY: sb.append(" 12key"); break;
            default: sb.append(" keys="); sb.append(keyboard); break;
        }
        switch (keyboardHidden) { 
            case KEYBOARDHIDDEN_UNDEFINED: sb.append("/?"); break;
            case KEYBOARDHIDDEN_NO: sb.append("/v"); break;
            case KEYBOARDHIDDEN_YES: sb.append("/h"); break;
            case KEYBOARDHIDDEN_SOFT: sb.append("/s"); break;
            default: sb.append("/"); sb.append(keyboardHidden); break;
        }
        switch (hardKeyboardHidden) {//
            case HARDKEYBOARDHIDDEN_UNDEFINED: sb.append("/?"); break;
            case HARDKEYBOARDHIDDEN_NO: sb.append("/v"); break;
            case HARDKEYBOARDHIDDEN_YES: sb.append("/h"); break;
            default: sb.append("/"); sb.append(hardKeyboardHidden); break;
        }
        switch (navigation) {//
            case NAVIGATION_UNDEFINED: sb.append(" ?nav"); break;
            case NAVIGATION_NONAV: sb.append(" -nav"); break;
            case NAVIGATION_DPAD: sb.append(" dpad"); break;
            case NAVIGATION_TRACKBALL: sb.append(" tball"); break;
            case NAVIGATION_WHEEL: sb.append(" wheel"); break;
            default: sb.append(" nav="); sb.append(navigation); break;
        }
        switch (navigationHidden) {//
            case NAVIGATIONHIDDEN_UNDEFINED: sb.append("/?"); break;
            case NAVIGATIONHIDDEN_NO: sb.append("/v"); break;
            case NAVIGATIONHIDDEN_YES: sb.append("/h"); break;
            default: sb.append("/"); sb.append(navigationHidden); break;
        }
         * 
         * 
         * 
         */
        return config.keyboard == Configuration.KEYBOARD_NOKEYS|| config.hardKeyboardHidden == Configuration.HARDKEYBOARDHIDDEN_YES;
    }










}