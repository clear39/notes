
//  @   frameworks/base/core/java/android/os/UEventObserver.java

/**
 * Representation of a UEvent.
 */
public static final class UEvent {
    // collection of key=value pairs parsed from the uevent message
    private final HashMap<String,String> mMap = new HashMap<String,String>();

    public UEvent(String message) {
        int offset = 0;
        int length = message.length();

        while (offset < length) {
            int equals = message.indexOf('=', offset);
            int at = message.indexOf('\0', offset);
            if (at < 0) break;

            if (equals > offset && equals < at) {
                // key is before the equals sign, and value is after
                mMap.put(message.substring(offset, equals),message.substring(equals + 1, at));
            }

            offset = at + 1;
        }
    }

    public String get(String key) {
        return mMap.get(key);
    }

    public String get(String key, String defaultValue) {
        String result = mMap.get(key);
        return (result == null ? defaultValue : result);
    }

    public String toString() {
        return mMap.toString();
    }
}









private static final class UEventThread extends Thread {

    public UEventThread() {
        super("UEventObserver");
    }

    /**
     * UEventObserver.getThread()
     * --> sThread.start();
     */
    @Override
    public void run() {
        /**
         * @    frameworks/base/core/jni/android_os_UEventObserver.cpp
         */
        nativeSetup();

        while (true) {
            /**
             * @    frameworks/base/core/jni/android_os_UEventObserver.cpp
             */
            String message = nativeWaitForNextEvent();
            if (message != null) {
                if (DEBUG) {
                    Log.d(TAG, message);
                }
                sendEvent(message);
            }
        }
    }


    private void sendEvent(String message) {
        synchronized (mKeysAndObservers) {
            final int N = mKeysAndObservers.size();
            for (int i = 0; i < N; i += 2) {
                final String key = (String)mKeysAndObservers.get(i);
                if (message.contains(key)) {
                    final UEventObserver observer = (UEventObserver)mKeysAndObservers.get(i + 1);
                    mTempObserversToSignal.add(observer);
                }
            }
        }

        if (!mTempObserversToSignal.isEmpty()) {
            final UEvent event = new UEvent(message);
            final int N = mTempObserversToSignal.size();
            for (int i = 0; i < N; i++) {
                final UEventObserver observer = mTempObserversToSignal.get(i);
                observer.onUEvent(event);
            }
            mTempObserversToSignal.clear();
        }
    }


    /**
     * UEventObserver.startObserving()
     * --> UEventThread.addObserver(match, this);
     */
    public void addObserver(String match, UEventObserver observer) {
        synchronized (mKeysAndObservers) {
            /**
             * private final ArrayList<Object> mKeysAndObservers = new ArrayList<Object>();
             */
            mKeysAndObservers.add(match);
            mKeysAndObservers.add(observer);
            /**
             * @    frameworks/base/core/jni/android_os_UEventObserver.cpp
             */
            nativeAddMatch(match);
        }
    }

}



static void nativeSetup(JNIEnv *env, jclass clazz) {
    /**
     * @    hardware/libhardware_legacy/uevent.c
     * 初始化了 NETLINK_KOBJECT_UEVENT socket 监听
     */
    if (!uevent_init()) {
        jniThrowException(env, "java/lang/RuntimeException","Unable to open socket for UEventObserver");
    }
}


static jstring nativeWaitForNextEvent(JNIEnv *env, jclass clazz) {
    char buffer[1024];

    for (;;) {
        /**
         * 
         */
        int length = uevent_next_event(buffer, sizeof(buffer) - 1);
        if (length <= 0) {
            return NULL;
        }
        buffer[length] = '\0';

        ALOGV("Received uevent message: %s", buffer);

        if (isMatch(buffer, length)) {
            // Assume the message is ASCII.
            jchar message[length];
            for (int i = 0; i < length; i++) {
                message[i] = buffer[i];
            }
            return env->NewString(message, length);
        }
    }
}

static bool isMatch(const char* buffer, size_t length) {
    AutoMutex _l(gMatchesMutex);

    for (size_t i = 0; i < gMatches.size(); i++) {
        const String8& match = gMatches.itemAt(i);

        // Consider all zero-delimited fields of the buffer.
        const char* field = buffer;
        const char* end = buffer + length + 1;
        do {
            if (strstr(field, match.string())) {
                ALOGV("Matched uevent message with pattern: %s", match.string());
                return true;
            }
            field += strlen(field) + 1;
        } while (field != end);
    }
    return false;
}


static void nativeAddMatch(JNIEnv* env, jclass clazz, jstring matchStr) {
    ScopedUtfChars match(env, matchStr);

    AutoMutex _l(gMatchesMutex);
    gMatches.add(String8(match.c_str()));
}
