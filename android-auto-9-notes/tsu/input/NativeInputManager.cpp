
class NativeInputManager : public virtual RefBase,
    public virtual InputReaderPolicyInterface,
    public virtual InputDispatcherPolicyInterface,
    public virtual PointerControllerPolicyInterface {


    }



//  @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/base/services/core/jni/com_android_server_input_InputManagerService.cpp

/**
 * 
 * 
 * 由于 NativeInputManager 实现 InputReaderPolicyInterface
 * 
*/
void NativeInputManager::notifyInputDevicesChanged(const Vector<InputDeviceInfo>& inputDevices) {
    ATRACE_CALL();
    JNIEnv* env = jniEnv();

    size_t count = inputDevices.size();
    jobjectArray inputDevicesObjArray = env->NewObjectArray(count, gInputDeviceClassInfo.clazz, NULL);
    if (inputDevicesObjArray) {
        bool error = false;
        for (size_t i = 0; i < count; i++) {
            jobject inputDeviceObj = android_view_InputDevice_create(env, inputDevices.itemAt(i));
            if (!inputDeviceObj) {
                error = true;
                break;
            }

            env->SetObjectArrayElement(inputDevicesObjArray, i, inputDeviceObj);
            env->DeleteLocalRef(inputDeviceObj);
        }

        if (!error) {
            env->CallVoidMethod(mServiceObj, gServiceClassInfo.notifyInputDevicesChanged,inputDevicesObjArray);
        }

        env->DeleteLocalRef(inputDevicesObjArray);
    }

    checkAndClearExceptionFromCallback(env, "notifyInputDevicesChanged");
}