/* 
 * ACodec 被封装在 MediaCodec 中，而 MediaCodec 又被封装在 NuPlayer::Decoder 中
 * 这里重点分析 AHierarchicalStateMachine 机制，类似与Java层中StateMachine机制
 */
//  @frameworks/av/media/libstagefright/ACodec.cpp
struct ACodec : public AHierarchicalStateMachine, public CodecBase {

    // AHierarchicalStateMachine implements the message handling
    virtual void onMessageReceived(const sp<AMessage> &msg) {
        handleMessage(msg);
    }
}


struct CodecBase : public AHandler, /* static */ ColorUtils {}




/**
 * AState 和 AHierarchicalStateMachine 匹配一起用
 * 
 */
//  @frameworks/av/media/libstagefright/foundation/AHierarchicalStateMachine.cpp
struct AState : public RefBase {
    sp<AState> mParentState;


    AState(const sp<AState> &parentState = NULL);

    virtual void stateEntered();

    virtual void stateExited();
    /**
     * 该方法由子类实现，在 AHierarchicalStateMachine 中 handleMessage 循环调用，
     * 直到 onMessageReceived返回true 或者  顶层的 父类AState 为空
     */
    virtual bool onMessageReceived(const sp<AMessage> &msg) = 0;  
}
struct AHierarchicalStateMachine {}


void AHierarchicalStateMachine::handleMessage(const sp<AMessage> &msg) {
    sp<AState> save = mState;

    sp<AState> cur = mState;
    /**
     * 循环调用，直到 onMessageReceived返回true 或者  顶层的 父类AState 为空
     */ 
    while (cur != NULL && !cur->onMessageReceived(msg)) {
        // If you claim not to have handled the message you shouldn't
        // have called setState...
        CHECK(save == mState);

        cur = cur->parentState();
    }

    if (cur != NULL) {
        return;
    }

    ALOGW("Warning message %s unhandled in root state.",msg->debugString().c_str());
}

void AHierarchicalStateMachine::changeState(const sp<AState> &state) {
    if (state == mState) {
        // Quick exit for the easy case.
        return;
    }

    /**
     * 将当前mState压入到Vector A中，直到父类为null；
     */ 
    Vector<sp<AState> > A;
    sp<AState> cur = mState;
    for (;;) {
        A.push(cur);
        if (cur == NULL) {
            break;
        }
        cur = cur->parentState();
    }

    /**
     * 将state压入到Vector B中，直到父类为null；
     */ 
    Vector<sp<AState> > B;
    cur = state;
    for (;;) {
        B.push(cur);
        if (cur == NULL) {
            break;
        }
        cur = cur->parentState();
    }

    // Remove the common tail.
    while (A.size() > 0 && B.size() > 0 && A.top() == B.top()) {
        A.pop();
        B.pop();
    }

    mState = state;

    /**
     * 从子到父依次调用 stateExited
     */ 
    for (size_t i = 0; i < A.size(); ++i) {
        A.editItemAt(i)->stateExited();
    }

    /**
     * 从父到子依次调用 stateEntered
     */ 
    for (size_t i = B.size(); i > 0;) {
        i--;
        B.editItemAt(i)->stateEntered();
    }
}



