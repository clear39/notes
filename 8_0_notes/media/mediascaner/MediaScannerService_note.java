public class MediaScannerService extends Service implements Runnable {
 
    @Override
    public void onCreate() {
        PowerManager pm = (PowerManager)getSystemService(Context.POWER_SERVICE);
        mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, TAG);
        StorageManager storageManager = (StorageManager)getSystemService(Context.STORAGE_SERVICE);
        mExternalStoragePaths = storageManager.getVolumePaths();

        // Start up the thread running the service.  Note that we create a
        // separate thread because the service normally runs in the process's
        // main thread, which we don't want to block.
        Thread thr = new Thread(null, this, "MediaScannerService");
        thr.start();
    }


    @Override
    public void run() {
        // reduce priority below other background threads to avoid interfering
        // with other services at boot time.
        //降低线程等级
        Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND + Process.THREAD_PRIORITY_LESS_FAVORABLE);
        Looper.prepare();

        mServiceLooper = Looper.myLooper();
        mServiceHandler = new ServiceHandler();

        Looper.loop();
    }


    //触发扫描
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        while (mServiceHandler创建 == null) {
            synchronized (this) {
                try {
                    wait(100);//等待mServiceHandler创建
                } catch (InterruptedException e) {
                }
            }
        }

        if (intent == null) {
            Log.e(TAG, "Intent is null in onStartCommand: ", new NullPointerException());
            return Service.START_NOT_STICKY;
        }

        Message msg = mServiceHandler.obtainMessage();
        msg.arg1 = startId;//用于扫描完成，退出service
        msg.obj = intent.getExtras();
        mServiceHandler.sendMessage(msg);

        // Try again later if we are killed before we can finish scanning.
        return Service.START_REDELIVER_INTENT;
    }


}