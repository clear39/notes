/**
 * This class represents an active client session.  There is generally one
 * Session object per process that is interacting with the window manager.
 */
// Needs to be public and not final so we can mock during tests...sucks I know :(
public class Session extends IWindowSession.Stub
        implements IBinder.DeathRecipient {

 	@Override
    public int add(IWindow window, int seq, WindowManager.LayoutParams attrs,
            int viewVisibility, Rect outContentInsets, Rect outStableInsets,InputChannel outInputChannel) {
        return addToDisplay(window, seq, attrs, viewVisibility, Display.DEFAULT_DISPLAY,outContentInsets, outStableInsets, null /* outOutsets */, outInputChannel);
    }


     @Override
    public int addToDisplay(IWindow window, int seq, WindowManager.LayoutParams attrs,
            int viewVisibility, int displayId, Rect outContentInsets, Rect outStableInsets,
            Rect outOutsets, InputChannel outInputChannel) {
        return mService.addWindow(this, window, seq, attrs, viewVisibility, displayId,outContentInsets, outStableInsets, outOutsets, outInputChannel);
    }
}