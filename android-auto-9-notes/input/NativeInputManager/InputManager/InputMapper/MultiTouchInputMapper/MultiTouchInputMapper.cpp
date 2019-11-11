
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
/**
 *  mContext（InputReaderContext） 实在 InputReader 中创建 
*/
InputMapper::InputMapper(InputDevice* device) : mDevice(device), mContext(device->getContext()) {
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MultiTouchInputMapper::reset(nsecs_t when) {
   /**
    * inline InputDevice* getDevice() { return mDevice; }
    * 
    * frameworks/native/services/inputflinger/InputReader.h:1909:    
    * MultiTouchMotionAccumulator mMultiTouchMotionAccumulator;
    * 
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

void TouchInputMapper::sync(nsecs_t when) {
    const RawState* last = mRawStatesPending.isEmpty() ? &mCurrentRawState : &mRawStatesPending.top();

    // Push a new state.
    mRawStatesPending.push();
    RawState* next = &mRawStatesPending.editTop();
    next->clear();
    next->when = when;

    // Sync button state.
    next->buttonState = mTouchButtonAccumulator.getButtonState() | mCursorButtonAccumulator.getButtonState();

    // Sync scroll
    next->rawVScroll = mCursorScrollAccumulator.getRelativeVWheel();
    next->rawHScroll = mCursorScrollAccumulator.getRelativeHWheel();
    mCursorScrollAccumulator.finishSync();

    // Sync touch
    syncTouch(when, next);

    // Assign pointer ids.
    if (!mHavePointerIds) {
        assignPointerIds(last, next);
    }

#if DEBUG_RAW_EVENTS
    ALOGD("syncTouch: pointerCount %d -> %d, touching ids 0x%08x -> 0x%08x, "
            "hovering ids 0x%08x -> 0x%08x",
            last->rawPointerData.pointerCount,
            next->rawPointerData.pointerCount,
            last->rawPointerData.touchingIdBits.value,
            next->rawPointerData.touchingIdBits.value,
            last->rawPointerData.hoveringIdBits.value,
            next->rawPointerData.hoveringIdBits.value);
#endif

    processRawTouches(false /*timeout*/);
}


void MultiTouchInputMapper::syncTouch(nsecs_t when, RawState* outState) {
    size_t inCount = mMultiTouchMotionAccumulator.getSlotCount();
    size_t outCount = 0;
    BitSet32 newPointerIdBits;
    mHavePointerIds = true;

    for (size_t inIndex = 0; inIndex < inCount; inIndex++) {
        const MultiTouchMotionAccumulator::Slot* inSlot = mMultiTouchMotionAccumulator.getSlot(inIndex);
        if (!inSlot->isInUse()) {
            continue;
        }

        if (outCount >= MAX_POINTERS) {
#if DEBUG_POINTERS
            ALOGD("MultiTouch device %s emitted more than maximum of %d pointers; "
                    "ignoring the rest.",
                    getDeviceName().string(), MAX_POINTERS);
#endif
            break; // too many fingers!
        }

        RawPointerData::Pointer& outPointer = outState->rawPointerData.pointers[outCount];
        outPointer.x = inSlot->getX();
        outPointer.y = inSlot->getY();
        outPointer.pressure = inSlot->getPressure();
        outPointer.touchMajor = inSlot->getTouchMajor();
        outPointer.touchMinor = inSlot->getTouchMinor();
        outPointer.toolMajor = inSlot->getToolMajor();
        outPointer.toolMinor = inSlot->getToolMinor();
        outPointer.orientation = inSlot->getOrientation();
        outPointer.distance = inSlot->getDistance();
        outPointer.tiltX = 0;
        outPointer.tiltY = 0;

        outPointer.toolType = inSlot->getToolType();
        if (outPointer.toolType == AMOTION_EVENT_TOOL_TYPE_UNKNOWN) {
            outPointer.toolType = mTouchButtonAccumulator.getToolType();
            if (outPointer.toolType == AMOTION_EVENT_TOOL_TYPE_UNKNOWN) {
                outPointer.toolType = AMOTION_EVENT_TOOL_TYPE_FINGER;
            }
        }

        bool isHovering = mTouchButtonAccumulator.getToolType() != AMOTION_EVENT_TOOL_TYPE_MOUSE
                && (mTouchButtonAccumulator.isHovering() || (mRawPointerAxes.pressure.valid && inSlot->getPressure() <= 0));
        outPointer.isHovering = isHovering;

        // Assign pointer id using tracking id if available.
        if (mHavePointerIds) {
            int32_t trackingId = inSlot->getTrackingId();
            int32_t id = -1;
            if (trackingId >= 0) {
                for (BitSet32 idBits(mPointerIdBits); !idBits.isEmpty(); ) {
                    uint32_t n = idBits.clearFirstMarkedBit();
                    if (mPointerTrackingIdMap[n] == trackingId) {
                        id = n;
                    }
                }

                if (id < 0 && !mPointerIdBits.isFull()) {
                    id = mPointerIdBits.markFirstUnmarkedBit();
                    mPointerTrackingIdMap[id] = trackingId;
                }
            }
            if (id < 0) {
                mHavePointerIds = false;
                outState->rawPointerData.clearIdBits();
                newPointerIdBits.clear();
            } else {
                outPointer.id = id;
                outState->rawPointerData.idToIndex[id] = outCount;
                outState->rawPointerData.markIdBit(id, isHovering);
                newPointerIdBits.markBit(id);
            }
        }
        outCount += 1;
    }

    outState->deviceTimestamp = mMultiTouchMotionAccumulator.getDeviceTimestamp();
    outState->rawPointerData.pointerCount = outCount;
    mPointerIdBits = newPointerIdBits;

    mMultiTouchMotionAccumulator.finishSync();
}

void TouchInputMapper::processRawTouches(bool timeout) {
    if (mDeviceMode == DEVICE_MODE_DISABLED) {
        // Drop all input if the device is disabled.
        mCurrentRawState.clear();
        mRawStatesPending.clear();
        return;
    }

    // Drain any pending touch states. The invariant here is that the mCurrentRawState is always
    // valid and must go through the full cook and dispatch cycle. This ensures that anything
    // touching the current state will only observe the events that have been dispatched to the
    // rest of the pipeline.
    const size_t N = mRawStatesPending.size();
    size_t count;
    for(count = 0; count < N; count++) {
        const RawState& next = mRawStatesPending[count];

        // A failure to assign the stylus id means that we're waiting on stylus data
        // and so should defer the rest of the pipeline.
        if (assignExternalStylusId(next, timeout)) {
            break;
        }

        // All ready to go.
        clearStylusDataPendingFlags();
        mCurrentRawState.copyFrom(next);
        if (mCurrentRawState.when < mLastRawState.when) {
            mCurrentRawState.when = mLastRawState.when;
        }
        cookAndDispatch(mCurrentRawState.when);
    }
    if (count != 0) {
        mRawStatesPending.removeItemsAt(0, count);
    }

    if (mExternalStylusDataPending) {
        if (timeout) {
            nsecs_t when = mExternalStylusFusionTimeout - STYLUS_DATA_LATENCY;
            clearStylusDataPendingFlags();
            mCurrentRawState.copyFrom(mLastRawState);
#if DEBUG_STYLUS_FUSION
            ALOGD("Timeout expired, synthesizing event with new stylus data");
#endif
            cookAndDispatch(when);
        } else if (mExternalStylusFusionTimeout == LLONG_MAX) {
            mExternalStylusFusionTimeout = mExternalStylusState.when + TOUCH_DATA_TIMEOUT;
            getContext()->requestTimeoutAtTime(mExternalStylusFusionTimeout);
        }
    }
}


void TouchInputMapper::dispatchVirtualKey(nsecs_t when, uint32_t policyFlags,int32_t keyEventAction, int32_t keyEventFlags) {
    int32_t keyCode = mCurrentVirtualKey.keyCode;
    int32_t scanCode = mCurrentVirtualKey.scanCode;
    nsecs_t downTime = mCurrentVirtualKey.downTime;
    int32_t metaState = mContext->getGlobalMetaState();
    policyFlags |= POLICY_FLAG_VIRTUAL;

    NotifyKeyArgs args(when, getDeviceId(), AINPUT_SOURCE_KEYBOARD, policyFlags,keyEventAction, keyEventFlags, keyCode, scanCode, metaState, downTime);
    getListener()->notifyKey(&args);
}