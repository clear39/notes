class WindowState extends WindowContainer<WindowState> implements WindowManagerPolicy.WindowState {



    WindowState(WindowManagerService service, Session s, IWindow c, WindowToken token,
           WindowState parentWindow, int appOp, int seq, WindowManager.LayoutParams a,
           int viewVisibility, int ownerId, boolean ownerCanAddInternalSystemWindow) {
        。。。。。。
        mInputWindowHandle = new InputWindowHandle(mAppToken != null ? mAppToken.mInputApplicationHandle : null, this, c,getDisplayId());
    }


	void openInputChannel(InputChannel outInputChannel) {
        if (mInputChannel != null) {
            throw new IllegalStateException("Window already has an input channel.");
        }
        String name = getName();
        //本地层创建一对socket,并且封装在 InputChannel 中 并返回
        InputChannel[] inputChannels = InputChannel.openInputChannelPair(name);
        mInputChannel = inputChannels[0];//这个传送个input服务
        mClientChannel = inputChannels[1];//这个传送给应用apk
        mInputWindowHandle.inputChannel = inputChannels[0];
        if (outInputChannel != null) {
            mClientChannel.transferTo(outInputChannel);
            mClientChannel.dispose();
            mClientChannel = null;
        } else {
            // If the window died visible, we setup a dummy input channel, so that taps
            // can still detected by input monitor channel, and we can relaunch the app.
            // Create dummy event receiver that simply reports all events as handled.
            mDeadWindowEventReceiver = new DeadWindowEventReceiver(mClientChannel);
        }
        //这里进行注册，mInputWindowHandle封装了 WindowState 所哟属性
        mService.mInputManager.registerInputChannel(mInputChannel, mInputWindowHandle);
    }

}