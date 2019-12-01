package com.screen.record.media;

import android.media.MediaCodec;
import android.view.Surface;

public abstract class MediaCodecBase {

    protected MediaCodec mEncoder;
    protected Surface mSurface;

    protected boolean mIsRun = false;
    protected OnUpdateData mOnUpdateData;

    public abstract void prepare();

    public abstract void release();

    public abstract void getBuffer();

    public Surface getSurface(){
        return mSurface;
    }

    public synchronized  void setRunStatus(boolean isRun){
        mIsRun = isRun;
    }


    public void setOnUpdateData(OnUpdateData l){
        mOnUpdateData = l;
    }



    public static String mediaCodecBufferInfoFlags2Str(int flags){
        StringBuilder sb = new StringBuilder();
        if( (flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) > 0){
            sb.append("BUFFER_FLAG_KEY_FRAME");
            sb.append("|");
        }

        if( (flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) > 0){
            sb.append("BUFFER_FLAG_CODEC_CONFIG");
            sb.append("|");
        }

        if( (flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) > 0){
            sb.append("BUFFER_FLAG_END_OF_STREAM");
            sb.append("|");
        }

        if( (flags & MediaCodec.BUFFER_FLAG_PARTIAL_FRAME) > 0){
            sb.append("BUFFER_FLAG_PARTIAL_FRAME");
            sb.append("|");
        }
        /*
        if( (flags & MediaCodec.BUFFER_FLAG_MUXER_DATA) > 0){
            sb.append("BUFFER_FLAG_MUXER_DATA");
        }
        */
    }


    public interface OnUpdateData{
        public void onUpdateData(H264data data);
    }
}
