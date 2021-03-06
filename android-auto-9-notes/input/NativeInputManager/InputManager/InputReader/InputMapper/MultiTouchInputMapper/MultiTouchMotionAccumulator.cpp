//  @   frameworks/native/services/inputflinger/InputReader.h:896:
class MultiTouchMotionAccumulator {

}


//  @  frameworks/native/services/inputflinger/InputReader.cpp
MultiTouchMotionAccumulator::MultiTouchMotionAccumulator() :
        mCurrentSlot(-1), mSlots(NULL), mSlotCount(0), mUsingSlotsProtocol(false),
        mHaveStylus(false), mDeviceTimestamp(0) {
}


void MultiTouchMotionAccumulator::configure(InputDevice* device,size_t slotCount, bool usingSlotsProtocol) {
    mSlotCount = slotCount;
    mUsingSlotsProtocol = usingSlotsProtocol;
    mHaveStylus = device->hasAbsoluteAxis(ABS_MT_TOOL_TYPE);

    delete[] mSlots;
    mSlots = new Slot[slotCount];
}


void MultiTouchMotionAccumulator::reset(InputDevice* device) {
    // Unfortunately there is no way to read the initial contents of the slots.
    // So when we reset the accumulator, we must assume they are all zeroes.
    if (mUsingSlotsProtocol) {
        // Query the driver for the current slot index and use it as the initial slot
        // before we start reading events from the device.  It is possible that the
        // current slot index will not be the same as it was when the first event was
        // written into the evdev buffer, which means the input mapper could start
        // out of sync with the initial state of the events in the evdev buffer.
        // In the extremely unlikely case that this happens, the data from
        // two slots will be confused until the next ABS_MT_SLOT event is received.
        // This can cause the touch point to "jump", but at least there will be
        // no stuck touches.
        int32_t initialSlot;
        status_t status = device->getEventHub()->getAbsoluteAxisValue(device->getId(),ABS_MT_SLOT, &initialSlot);
        if (status) {
            ALOGD("Could not retrieve current multitouch slot index.  status=%d", status);
            initialSlot = -1;
        }
        clearSlots(initialSlot);
    } else {
        clearSlots(-1);
    }
    mDeviceTimestamp = 0;
}









////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MultiTouchMotionAccumulator::process(const RawEvent* rawEvent) {
    if (rawEvent->type == EV_ABS) {
        bool newSlot = false;
        if (mUsingSlotsProtocol) {
            if (rawEvent->code == ABS_MT_SLOT) {
                mCurrentSlot = rawEvent->value;
                newSlot = true;
            }
        } else if (mCurrentSlot < 0) {
            mCurrentSlot = 0;
        }

        if (mCurrentSlot < 0 || size_t(mCurrentSlot) >= mSlotCount) {
#if DEBUG_POINTERS
            if (newSlot) {
                ALOGW("MultiTouch device emitted invalid slot index %d but it "
                        "should be between 0 and %zd; ignoring this slot.",
                        mCurrentSlot, mSlotCount - 1);
            }
#endif
        } else {
            Slot* slot = &mSlots[mCurrentSlot];

            switch (rawEvent->code) {
            case ABS_MT_POSITION_X:
                slot->mInUse = true;
                slot->mAbsMTPositionX = rawEvent->value;
                break;
            case ABS_MT_POSITION_Y:
                slot->mInUse = true;
                slot->mAbsMTPositionY = rawEvent->value;
                break;
            case ABS_MT_TOUCH_MAJOR:
                slot->mInUse = true;
                slot->mAbsMTTouchMajor = rawEvent->value;
                break;
            case ABS_MT_TOUCH_MINOR:
                slot->mInUse = true;
                slot->mAbsMTTouchMinor = rawEvent->value;
                slot->mHaveAbsMTTouchMinor = true;
                break;
            case ABS_MT_WIDTH_MAJOR:
                slot->mInUse = true;
                slot->mAbsMTWidthMajor = rawEvent->value;
                break;
            case ABS_MT_WIDTH_MINOR:
                slot->mInUse = true;
                slot->mAbsMTWidthMinor = rawEvent->value;
                slot->mHaveAbsMTWidthMinor = true;
                break;
            case ABS_MT_ORIENTATION:
                slot->mInUse = true;
                slot->mAbsMTOrientation = rawEvent->value;
                break;
            case ABS_MT_TRACKING_ID:
                if (mUsingSlotsProtocol && rawEvent->value < 0) {
                    // The slot is no longer in use but it retains its previous contents,
                    // which may be reused for subsequent touches.
                    slot->mInUse = false;
                } else {
                    slot->mInUse = true;
                    slot->mAbsMTTrackingId = rawEvent->value;
                }
                break;
            case ABS_MT_PRESSURE:
                slot->mInUse = true;
                slot->mAbsMTPressure = rawEvent->value;
                break;
            case ABS_MT_DISTANCE:
                slot->mInUse = true;
                slot->mAbsMTDistance = rawEvent->value;
                break;
            case ABS_MT_TOOL_TYPE:
                slot->mInUse = true;
                slot->mAbsMTToolType = rawEvent->value;
                slot->mHaveAbsMTToolType = true;
                break;
            }
        }
    } else if (rawEvent->type == EV_SYN && rawEvent->code == SYN_MT_REPORT) {
        // MultiTouch Sync: The driver has returned all data for *one* of the pointers.
        mCurrentSlot += 1;
    } else if (rawEvent->type == EV_MSC && rawEvent->code == MSC_TIMESTAMP) {
        mDeviceTimestamp = rawEvent->value;
    }
}

void MultiTouchMotionAccumulator::finishSync() {
    if (!mUsingSlotsProtocol) {
        clearSlots(-1);
    }
}
