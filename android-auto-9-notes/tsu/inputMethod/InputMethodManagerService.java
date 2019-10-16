

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/services/core/java/com/android/server/InputMethodManagerService.java
public class InputMethodManagerService extends IInputMethodManager.Stub
        implements ServiceConnection, Handler.Callback {

    /***
     * WindowManagerGlobal.getWindowSession-->WindowManagerService.openSession--> new Session
     * --> InputMethodManagerService.addClient
     */
    @Override
    public void addClient(IInputMethodClient client,
            IInputContext inputContext, int uid, int pid) {
        if (!calledFromValidUser()) {
            return;
        }
        synchronized (mMethodMap) {
            mClients.put(client.asBinder(), new ClientState(client,inputContext, uid, pid));
        }
    }
}