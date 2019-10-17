

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/services/java/com/android/server/SystemServer.java

private void startOtherServices() {
    ......
    if (mFactoryTestMode != FactoryTest.FACTORY_TEST_LOW_LEVEL) {
        traceBeginAndSlog("StartInputMethodManagerLifecycle");
        mSystemServiceManager.startService(InputMethodManagerService.Lifecycle.class);
        traceEnd();

        if(false){
            traceBeginAndSlog("StartAccessibilityManagerService");
            try {
                ServiceManager.addService(Context.ACCESSIBILITY_SERVICE,
                        new AccessibilityManagerService(context));
            } catch (Throwable e) {
                reportWtf("starting Accessibility Manager", e);
            }
            traceEnd();
        }
    }
    ......
}