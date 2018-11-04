
public class DecorView extends FrameLayout implements RootViewSurfaceTaker, WindowCallbacks {


    public android.view.SurfaceHolder.Callback2 willYouTakeTheSurface() {
        return mFeatureId < 0 ? mWindow.mTakeSurfaceCallback : null;
    }

    public InputQueue.Callback willYouTakeTheInputQueue() {
        return mFeatureId < 0 ? mWindow.mTakeInputQueueCallback : null;
    }

    
}