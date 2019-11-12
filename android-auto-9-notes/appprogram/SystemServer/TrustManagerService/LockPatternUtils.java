//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/base/core/java/com/android/internal/widget/LockPatternUtils.java
public class LockPatternUtils {
    
    public LockPatternUtils(Context context) {
        mContext = context;
        mContentResolver = context.getContentResolver();

        Looper looper = Looper.myLooper();
        mHandler = looper != null ? new Handler(looper) : null;
    }


}