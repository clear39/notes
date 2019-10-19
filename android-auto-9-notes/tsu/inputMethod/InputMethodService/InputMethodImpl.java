
//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/core/java/android/inputmethodservice/AbstractInputMethodService.java
public abstract class AbstractInputMethodImpl implements InputMethod {




}




//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/core/java/android/inputmethodservice/InputMethodService.java
public class InputMethodImpl extends AbstractInputMethodImpl {


    /**
     * {@inheritDoc}
     */
    @MainThread
    @Override
    public void showSoftInput(int flags, ResultReceiver resultReceiver) {
        if (DEBUG) Log.v(TAG, "showSoftInput() flags:" + HexDump.toHexString(flags));
        boolean wasVis = isInputViewShown();
        if (DEBUG) Log.v(TAG, "showSoftInput() wasVis:" + wasVis);
        /***
         * InputMethodService.dispatchOnShowInputRequested
         */
        if (dispatchOnShowInputRequested(flags, false) ) {
            try {
                showWindow(true);
            } catch (BadTokenException e) {
                // We have ignored BadTokenException here since Jelly Bean MR-2 (API Level 18).
                // We could ignore BadTokenException in InputMethodService#showWindow() instead,
                // but it may break assumptions for those who override #showWindow() that we can
                // detect errors in #showWindow() by checking BadTokenException.
                // TODO: Investigate its feasibility.  Update JavaDoc of #showWindow() of
                // whether it's OK to override #showWindow() or not.
            }
        }
        clearInsetOfPreviousIme();
        // If user uses hard keyboard, IME button should always be shown.
        mImm.setImeWindowStatus(mToken, mStartInputToken,
                mapToImeWindowStatus(isInputViewShown()), mBackDisposition);
        if (resultReceiver != null) {
            resultReceiver.send(wasVis != isInputViewShown()
                    ? InputMethodManager.RESULT_SHOWN
                    : (wasVis ? InputMethodManager.RESULT_UNCHANGED_SHOWN
                            : InputMethodManager.RESULT_UNCHANGED_HIDDEN), null);
        }
    }


}