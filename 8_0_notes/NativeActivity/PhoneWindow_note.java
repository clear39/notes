
public class PhoneWindow extends Window implements MenuBuilder.Callback {

	
	Callback2 mTakeSurfaceCallback;
    InputQueue.Callback mTakeInputQueueCallback;

	@Override
    public void takeSurface(Callback2 callback) {
        mTakeSurfaceCallback = callback;
    }

    public void takeInputQueue(InputQueue.Callback callback) {
        mTakeInputQueueCallback = callback;
    }



}