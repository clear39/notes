

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/services/core/java/com/android/server/wm/Session.java
class Session extends IWindowSession.Stub implements IBinder.DeathRecipient {

    public Session(WindowManagerService service, IWindowSessionCallback callback,
            IInputMethodClient client, IInputContext inputContext) {
        mService = service;
        mCallback = callback;
        mClient = client;
        mUid = Binder.getCallingUid();
        mPid = Binder.getCallingPid();
        mLastReportedAnimatorScale = service.getCurrentAnimatorScale();
        mCanAddInternalSystemWindow = service.mContext.checkCallingOrSelfPermission(INTERNAL_SYSTEM_WINDOW) == PERMISSION_GRANTED;
        mCanHideNonSystemOverlayWindows = service.mContext.checkCallingOrSelfPermission(HIDE_NON_SYSTEM_OVERLAY_WINDOWS) == PERMISSION_GRANTED;
        mCanAcquireSleepToken = service.mContext.checkCallingOrSelfPermission(DEVICE_POWER) == PERMISSION_GRANTED;
        mShowingAlertWindowNotificationAllowed = mService.mShowAlertWindowNotifications;
        mDragDropController = mService.mDragDropController;
        StringBuilder sb = new StringBuilder();
        sb.append("Session{");
        sb.append(Integer.toHexString(System.identityHashCode(this)));
        sb.append(" ");
        sb.append(mPid);
        if (mUid < Process.FIRST_APPLICATION_UID) {
            sb.append(":");
            sb.append(mUid);
        } else {
            sb.append(":u");
            sb.append(UserHandle.getUserId(mUid));
            sb.append('a');
            sb.append(UserHandle.getAppId(mUid));
        }
        sb.append("}");
        mStringName = sb.toString();

        synchronized (mService.mWindowMap) {
            if (mService.mInputMethodManager == null && mService.mHaveInputMethods) {
                IBinder b = ServiceManager.getService(Context.INPUT_METHOD_SERVICE);
                mService.mInputMethodManager = IInputMethodManager.Stub.asInterface(b);
            }
        }
        long ident = Binder.clearCallingIdentity();
        try {
            // Note: it is safe to call in to the input method manager
            // here because we are not holding our lock.
            if (mService.mInputMethodManager != null) {
                mService.mInputMethodManager.addClient(client, inputContext,mUid, mPid);
            } else {
                client.setUsingInputMethod(false);
            }
            client.asBinder().linkToDeath(this, 0);
        } catch (RemoteException e) {
            // The caller has died, so we can just forget about this.
            try {
                if (mService.mInputMethodManager != null) {
                    mService.mInputMethodManager.removeClient(client);
                }
            } catch (RemoteException ee) {
            }
        } finally {
            Binder.restoreCallingIdentity(ident);
        }
    }
}