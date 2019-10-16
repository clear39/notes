

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

}