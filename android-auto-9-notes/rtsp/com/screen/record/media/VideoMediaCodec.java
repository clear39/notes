package com.screen.record.media;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Build;
import android.util.Log;

import com.screen.record.Constant;

import java.io.IOException;
import java.nio.ByteBuffer;

public class VideoMediaCodec extends MediaCodecBase {

    private static final String TAG = "VideoMediaCodec";
    private static final boolean DBG = true;

    private int TIMEOUT_USEC = 12000;

    @Override
    public void prepare() {
        initMediaCodec();
    }


    private void initMediaCodec() {
        try {
            MediaFormat format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, Constant.VIDEO_WIDTH, Constant.VIDEO_HEIGHT);
            format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
            format.setInteger(MediaFormat.KEY_BIT_RATE, Constant.VIDEO_BITRATE);
            format.setInteger(MediaFormat.KEY_FRAME_RATE, Constant.VIDEO_FRAMERATE);
            format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, Constant.VIDEO_IFRAME_INTER);
            //MediaCodecList.
            mEncoder = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
            mEncoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            mSurface = mEncoder.createInputSurface();
            mEncoder.start();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void getBuffer() {

        while (mIsRun) {
            if (mEncoder == null)
                break;
            try {
                MediaCodec.BufferInfo mBufferInfo = new MediaCodec.BufferInfo();
                long startTime = System.currentTimeMillis();
                int outputBufferIndex = mEncoder.dequeueOutputBuffer(mBufferInfo, TIMEOUT_USEC);
                if (DBG)
                    Log.i(TAG, "dequeueOutputBuffer: " + (System.currentTimeMillis() - startTime));
                if (DBG)
                    Log.i(TAG, "outputBufferIndex:" + outputBufferIndex + " BufferInfo: [" + mBufferInfo.offset + "," + mBufferInfo.size + "," + mBufferInfo.presentationTimeUs + "," + mBufferInfo.flags);
                while (outputBufferIndex >= 0) {
                    ByteBuffer outputBuffer = null;
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                        outputBuffer = mEncoder.getOutputBuffer(outputBufferIndex);
                    } else {
                        outputBuffer = mEncoder.getOutputBuffers()[outputBufferIndex];
                    }

                    byte[] outData = new byte[mBufferInfo.size];
                    outputBuffer.get(outData);
                    if (mBufferInfo.flags == MediaCodec.BUFFER_FLAG_CODEC_CONFIG) {  // BUFFER_FLAG_CODEC_CONFIG
                        configbyte = new byte[mBufferInfo.size];
                        configbyte = outData;
                    } else if (mBufferInfo.flags == MediaCodec.BUFFER_FLAG_KEY_FRAME) { //BUFFER_FLAG_KEY_FRAME
                        byte[] keyframe = new byte[mBufferInfo.size + configbyte.length];
                        System.arraycopy(configbyte, 0, keyframe, 0, configbyte.length);
                        System.arraycopy(outData, 0, keyframe, configbyte.length, outData.length);
                        if(mOnUpdateData != null){
                            mOnUpdateData.onUpdateData();
                        }
                        MainActivity.putData(keyframe, 1, mBufferInfo.presentationTimeUs * 1000L);
                    } else {
                        MainActivity.putData(outData, 2, mBufferInfo.presentationTimeUs * 1000L);
                    }
                    mEncoder.releaseOutputBuffer(outputBufferIndex, false);
                }
            } catch (Exception e) {

            }
        }

        try {
            mEncoder.stop();
            mEncoder.release();
            mEncoder = null;
        } catch (Exception e) {
            e.printStackTrace();
        }

    }

    @Override
    public void release() {
        setRunStatus(false);
    }

}
