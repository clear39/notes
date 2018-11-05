

/***
	ViewRootImpl 主要用于 WindowManagerService通讯
	IWindowSession mWindowSession 为 apk到WindowManagerService请求接口
	W mWindow 为WindowManagerService到apk回调接口
*/
public final class 	ViewRootImpl implements ViewParent,View.AttachInfo.Callbacks, ThreadedRenderer.DrawCallbacks {

	final IWindowSession mWindowSession; //这个由应用端到服务端的binder接口

	final W mWindow;//这个由服务端回调给应用端的binder接口


	public ViewRootImpl(Context context, Display display) {
        mContext = context;
        mWindowSession = WindowManagerGlobal.getWindowSession();
        ......
        mWindow = new W(this);
        ......
    }


    static class W extends IWindow.Stub {

    }

}