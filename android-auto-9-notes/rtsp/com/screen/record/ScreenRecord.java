package com.screen.record;


import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.WindowManager;

import com.screen.record.media.H264data;
import com.screen.record.media.MediaCodecBase;
import com.screen.record.media.VideoMediaCodec;


public class ScreenRecord implements Runnable , MediaCodecBase.OnUpdateData {


    private static final String TAG = "RecoderService";


    private VirtualDisplay mVirtualDisplay;

    private int mScreenDensity;

    private int mScreenWidth;
    private int mScreenHeight;
    private String mPackageName;
    private int mUid;
    private VideoMediaCodec mVideoMediaCodec;

    private Thread mRecoderThread;

    private Context mContext;


    public ScreenRecord(Context c){
        mContext = c;
        WindowManager wm = (WindowManager) mContext.getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
        DisplayMetrics metrics= new DisplayMetrics();
        wm.getDefaultDisplay().getMetrics(metrics);
        mScreenDensity = metrics.densityDpi;
        mScreenWidth = metrics.widthPixels;
        mScreenHeight = metrics.heightPixels;

        mPackageName = mContext.getApplicationContext().getPackageName();
        PackageManager packageManager = mContext.getApplicationContext().getPackageManager();
        ApplicationInfo aInfo;
        try {
            aInfo = packageManager.getApplicationInfo(mPackageName, 0);
            mUid = aInfo.uid;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "unable to look up package name", e);
            return;
        }
    }

    public void start() {
        mVideoMediaCodec = new VideoMediaCodec();
        mVideoMediaCodec.setOnUpdateData(this);
        mRecoderThread = new Thread(this);
        mRecoderThread.start();
    }



    public void release() {
        if(mVideoMediaCodec != null){
            mVideoMediaCodec.release();
            mVideoMediaCodec = null;
        }

        if(mRecoderThread != null){
            try {
                mRecoderThread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }finally {
                mRecoderThread = null;
            }
        }

        if(mVirtualDisplay != null){
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }
    }

    @Override
    public void run() {
        mVideoMediaCodec.prepare();
        DisplayManager dm = (DisplayManager) mContext.getApplicationContext().getSystemService(Context.DISPLAY_SERVICE);
        mVirtualDisplay = dm.createVirtualDisplay(TAG + "-display", mScreenWidth, mScreenHeight, mScreenDensity, mVideoMediaCodec.getSurface(),DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC);
        mVideoMediaCodec.setRunStatus(true);
        mVideoMediaCodec.getBuffer();
    }

    @Override
    public void onUpdateData(H264data data) {
        /**
         *
        if (h264Queue.size() >= queuesize) {
            h264Queue.poll();
        }
        h264Queue.add(data);

         */
    }
}
