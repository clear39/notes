

//  @   /home/xqli/tools/android-sdk/sources/android-28/android/inputmethodservice/IInputMethodWrapper.java
/***
 * 
 * IInputMethod 是由 InputMethodManagerService 绑定 对应输入法服务得到对应的binder 回调接口
 */
class IInputMethodWrapper extends IInputMethod.Stub implements HandlerCaller.Callback {
    /***
     * 
     * 这里 inputMethod 为 InputMethodImpl(默认构造函数，没有任何实现)
     * 
     */
    public IInputMethodWrapper(AbstractInputMethodService context, InputMethod inputMethod) {
        mTarget = new WeakReference<>(context);
        mContext = context.getApplicationContext();
        mCaller = new HandlerCaller(mContext, null, this, true /*asyncHandler*/);
        mInputMethod = new WeakReference<>(inputMethod);
        mTargetSdkVersion = context.getApplicationInfo().targetSdkVersion;
    }
}