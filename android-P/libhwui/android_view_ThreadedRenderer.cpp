
// 对应java层的ThreadedRenderer(frameworks/base/core/java/android/view/ThreadedRenderer.java)类

//	@	frameworks/base/core/jni/android_view_ThreadedRenderer.cpp


static jlong android_view_ThreadedRenderer_createRootRenderNode(JNIEnv* env, jobject clazz) {
    RootRenderNode* node = new RootRenderNode(env);
    node->incStrong(0);
    node->setName("RootRenderNode");
    return reinterpret_cast<jlong>(node);
}

//	rootRenderNodePtr 为 RootRenderNode 
static jlong android_view_ThreadedRenderer_createProxy(JNIEnv* env, jobject clazz,
        jboolean translucent, jlong rootRenderNodePtr) {
    RootRenderNode* rootRenderNode = reinterpret_cast<RootRenderNode*>(rootRenderNodePtr);
    ContextFactoryImpl factory(rootRenderNode);
    // 	@	
    return (jlong) new RenderProxy(translucent, rootRenderNode, &factory);
}

