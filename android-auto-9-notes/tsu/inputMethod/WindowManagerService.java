

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java

public class WindowManagerService{


    
    @Override
    public IWindowSession openSession(IWindowSessionCallback callback, IInputMethodClient client,IInputContext inputContext) {
        if (client == null) throw new IllegalArgumentException("null client");
        if (inputContext == null) throw new IllegalArgumentException("null inputContext");
        Session session = new Session(this, callback, client, inputContext);
        return session;
    }




}


