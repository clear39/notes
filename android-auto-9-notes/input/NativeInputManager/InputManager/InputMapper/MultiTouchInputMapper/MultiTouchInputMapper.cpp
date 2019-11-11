
class TouchInputMapper : public InputMapper {

}


class MultiTouchInputMapper : public TouchInputMapper {

}


/**
 * 
 * 对于 cyttsp6_mt1 设备 # INPUT_DEVICE_CLASS_EXTERNAL | INPUT_DEVICE_CLASS_TOUCH_MT | INPUT_DEVICE_CLASS_TOUCH
 * 添加了该  Map
*/
//  @   frameworks/native/services/inputflinger/InputReader.cpp 
MultiTouchInputMapper::MultiTouchInputMapper(InputDevice* device) :
        TouchInputMapper(device) {
}


TouchInputMapper::TouchInputMapper(InputDevice* device) :
        InputMapper(device),
        mSource(0), mDeviceMode(DEVICE_MODE_DISABLED),
        mSurfaceWidth(-1), mSurfaceHeight(-1), mSurfaceLeft(0), mSurfaceTop(0),
        mPhysicalWidth(-1), mPhysicalHeight(-1), mPhysicalLeft(0), mPhysicalTop(0),
        mSurfaceOrientation(DISPLAY_ORIENTATION_0) {
}


//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/native/services/inputflinger/InputReader.cpp
InputMapper::InputMapper(InputDevice* device) : mDevice(device), mContext(device->getContext()) {
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MultiTouchInputMapper::reset(nsecs_t when) {
   /**
    * inline InputDevice* getDevice() { return mDevice; }
   */
    mMultiTouchMotionAccumulator.reset(getDevice());

    mPointerIdBits.clear();

    TouchInputMapper::reset(when);
}


void TouchInputMapper::reset(nsecs_t when) {
    mCursorButtonAccumulator.reset(getDevice());
    mCursorScrollAccumulator.reset(getDevice());
    mTouchButtonAccumulator.reset(getDevice());

    mPointerVelocityControl.reset();
    mWheelXVelocityControl.reset();
    mWheelYVelocityControl.reset();

    mRawStatesPending.clear();
    mCurrentRawState.clear();
    mCurrentCookedState.clear();
    mLastRawState.clear();
    mLastCookedState.clear();
    mPointerUsage = POINTER_USAGE_NONE;
    mSentHoverEnter = false;
    mHavePointerIds = false;
    mCurrentMotionAborted = false;
    mDownTime = 0;

    mCurrentVirtualKey.down = false;

    mPointerGesture.reset();
    mPointerSimple.reset();
    resetExternalStylus();

    if (mPointerController != NULL) {
        mPointerController->fade(PointerControllerInterface::TRANSITION_GRADUAL);
        mPointerController->clearSpots();
    }

    InputMapper::reset(when);
}








/////////////////////////////////////////////////////////////////////////////////////////////////////
void MultiTouchInputMapper::process(const RawEvent* rawEvent) {
    TouchInputMapper::process(rawEvent);

    mMultiTouchMotionAccumulator.process(rawEvent);
}


void TouchInputMapper::process(const RawEvent* rawEvent) {
    mCursorButtonAccumulator.process(rawEvent);
    mCursorScrollAccumulator.process(rawEvent);
    mTouchButtonAccumulator.process(rawEvent);

    if (rawEvent->type == EV_SYN && rawEvent->code == SYN_REPORT) {
        sync(rawEvent->when);
    }
}