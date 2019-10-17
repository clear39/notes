
//  @   /home/xqli/tools/android-sdk/sources/android-28/android/inputmethodservice/AbstractInputMethodService.java
public abstract class AbstractInputMethodService extends Service implements KeyEvent.Callback {

    @Override
    final public IBinder onBind(Intent intent) {
        if (mInputMethod == null) {
            mInputMethod = onCreateInputMethodInterface();
        }
        return new IInputMethodWrapper(this, mInputMethod);
    }

    

}