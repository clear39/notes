//	@packages/providers/MediaProvider/src/com/android/providers/media/MediaScannerService.java

private final class ServiceHandler extends Handler {

	/***
	 Message msg = mServiceHandler.obtainMessage();
        msg.arg1 = startId;//用于扫描完成，退出service
        msg.obj = intent.getExtras();
        mServiceHandler.sendMessage(msg);

	*/

	@Override
        public void handleMessage(Message msg) {
            Bundle arguments = (Bundle) msg.obj;
            if (arguments == null) {
                Log.e(TAG, "null intent, b/20953950");
                return;
            }
            // 当 filePath 不为空时，则表示对单个文件或者文件夹路径扫描
            String filePath = arguments.getString("filepath");
            
            try {
                if (filePath != null) {
                	//获取监听器的binder
                    IBinder binder = arguments.getIBinder("listener");
                    IMediaScannerListener listener =  (binder == null ? null : IMediaScannerListener.Stub.asInterface(binder));
                    Uri uri = null;
                    try {
                        uri = scanFile(filePath, arguments.getString("mimetype"));
                    } catch (Exception e) {
                        Log.e(TAG, "Exception scanning file", e);
                    }
                    if (listener != null) {
                        listener.scanCompleted(filePath, uri);//回调给触发扫描的客户端
                    }
                } else {
                    String volume = arguments.getString("volume");
                    String[] directories = null;

                    if (MediaProvider.INTERNAL_VOLUME.equals(volume)) {
                        // scan internal media storage
                        directories = new String[] {
                                Environment.getRootDirectory() + "/media",
                                Environment.getOemDirectory() + "/media",
                        };
                    }else if (MediaProvider.EXTERNAL_VOLUME.equals(volume)) {
                        // scan external storage volumes
                        if (getSystemService(UserManager.class).isDemoUser()) {
                            directories = ArrayUtils.appendElement(String.class,mExternalStoragePaths,Environment.getDataPreloadsMediaDirectory().getAbsolutePath());
                        } else {
                            directories = mExternalStoragePaths;
                        }
                    }

                    if (directories != null) {
                        if (false) Log.d(TAG, "start scanning volume " + volume + ": " + Arrays.toString(directories));
                        scan(directories, volume);
                        if (false) Log.d(TAG, "done scanning volume " + volume);
                    }
                }
            } catch (Exception e) {
                Log.e(TAG, "Exception in handleMessage", e);
            }

            stopSelf(msg.arg1);
        }

}