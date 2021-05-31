
//	frameworks/base/libs/hwui/renderthread/RenderProxy.cpp

RenderProxy::RenderProxy(bool translucent, RenderNode* rootRenderNode, IContextFactory* contextFactory)
        : mRenderThread(RenderThread::getInstance()), mContext(nullptr) {

    mContext = mRenderThread.queue().runSync([&]() -> CanvasContext* {
        return CanvasContext::create(mRenderThread, translucent, rootRenderNode, contextFactory);
    });
    
    mDrawFrameTask.setContext(&mRenderThread, mContext, rootRenderNode);
}