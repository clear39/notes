

class RootRenderNode : public RenderNode, ErrorHandler {
public:
    explicit RootRenderNode(JNIEnv* env) : RenderNode() {
        mLooper = Looper::getForThread();
        LOG_ALWAYS_FATAL_IF(!mLooper.get(), "Must create RootRenderNode on a thread with a looper!");
        env->GetJavaVM(&mVm);
    }
}