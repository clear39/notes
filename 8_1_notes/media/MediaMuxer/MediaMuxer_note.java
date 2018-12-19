//	@frameworks/base/media/java/android/media/MediaMuxer.java
final public class MediaMuxer {

	//构造函数最终都会调用到这里，进行c++层 初始化
	private void setUpMediaMuxer(@NonNull FileDescriptor fd, @Format int format) throws IOException {
        if (format != OutputFormat.MUXER_OUTPUT_MPEG_4 && format != OutputFormat.MUXER_OUTPUT_WEBM && format != OutputFormat.MUXER_OUTPUT_3GPP) {
            throw new IllegalArgumentException("format: " + format + " is invalid");
        }
        mNativeObject = nativeSetup(fd, format);
        mState = MUXER_STATE_INITIALIZED;
        mCloseGuard.open("release");
    }


    /**
     * Starts the muxer.
     * <p>Make sure this is called after {@link #addTrack} and before
     * {@link #writeSampleData}.</p>
     * @throws IllegalStateException If this method is called after {@link #start}
     * or Muxer is released
     */
    // 启动 muxer ，确保在调用 writeSampleData 前，进行调用
    public void start() {
        if (mNativeObject == 0) {
            throw new IllegalStateException("Muxer has been released!");
        }
        if (mState == MUXER_STATE_INITIALIZED) {
            nativeStart(mNativeObject);
            mState = MUXER_STATE_STARTED;
        } else {
            throw new IllegalStateException("Can't start due to wrong state.");
        }
    }






    public int addTrack(@NonNull MediaFormat format) {
        if (format == null) {
            throw new IllegalArgumentException("format must not be null.");
        }
        if (mState != MUXER_STATE_INITIALIZED) {
            throw new IllegalStateException("Muxer is not initialized.");
        }
        if (mNativeObject == 0) {
            throw new IllegalStateException("Muxer has been released!");
        }
        int trackIndex = -1;
        // Convert the MediaFormat into key-value pairs and send to the native.
        Map<String, Object> formatMap = format.getMap();

        String[] keys = null;
        Object[] values = null;
        int mapSize = formatMap.size();
        if (mapSize > 0) {
            keys = new String[mapSize];
            values = new Object[mapSize];
            int i = 0;
            for (Map.Entry<String, Object> entry : formatMap.entrySet()) {
                keys[i] = entry.getKey();
                values[i] = entry.getValue();
                ++i;
            }
            trackIndex = nativeAddTrack(mNativeObject, keys, values);
        } else {
            throw new IllegalArgumentException("format must not be empty.");
        }

        // Track index number is expected to incremented as addTrack succeed.
        // However, if format is invalid, it will get a negative trackIndex.
        if (mLastTrackIndex >= trackIndex) {
            throw new IllegalArgumentException("Invalid format.");
        }
        mLastTrackIndex = trackIndex;
        return trackIndex;
    }



    public void writeSampleData(int trackIndex, @NonNull ByteBuffer byteBuf, @NonNull BufferInfo bufferInfo) {
        if (trackIndex < 0 || trackIndex > mLastTrackIndex) {
            throw new IllegalArgumentException("trackIndex is invalid");
        }

        if (byteBuf == null) {
            throw new IllegalArgumentException("byteBuffer must not be null");
        }

        if (bufferInfo == null) {
            throw new IllegalArgumentException("bufferInfo must not be null");
        }
        if (bufferInfo.size < 0 || bufferInfo.offset < 0 || (bufferInfo.offset + bufferInfo.size) > byteBuf.capacity() || bufferInfo.presentationTimeUs < 0) {
            throw new IllegalArgumentException("bufferInfo must specify a" + " valid buffer offset, size and presentation time");
        }

        if (mNativeObject == 0) {
            throw new IllegalStateException("Muxer has been released!");
        }

        if (mState != MUXER_STATE_STARTED) {
            throw new IllegalStateException("Can't write, muxer is not started");
        }

        nativeWriteSampleData(mNativeObject, trackIndex, byteBuf,bufferInfo.offset, bufferInfo.size,bufferInfo.presentationTimeUs, bufferInfo.flags);
    }



}





static jlong android_media_MediaMuxer_native_setup(JNIEnv *env, jclass clazz, jobject fileDescriptor,jint format) {
    int fd = jniGetFDFromFileDescriptor(env, fileDescriptor);
    ALOGV("native_setup: fd %d", fd);

    // It appears that if an invalid file descriptor is passed through
    // binder calls, the server-side of the inter-process function call
    // is skipped. As a result, the check at the server-side to catch
    // the invalid file descritpor never gets invoked. This is to workaround
    // this issue by checking the file descriptor first before passing
    // it through binder call.
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        ALOGE("Fail to get File Status Flags err: %s", strerror(errno));
        jniThrowException(env, "java/lang/IllegalArgumentException","Invalid file descriptor");
        return 0;
    }

    // fd must be in read-write mode or write-only mode.
    if ((flags & (O_RDWR | O_WRONLY)) == 0) {
        ALOGE("File descriptor is not in read-write mode or write-only mode");
        jniThrowException(env, "java/io/IOException","File descriptor is not in read-write mode or write-only mode");
        return 0;
    }

    MediaMuxer::OutputFormat fileFormat = static_cast<MediaMuxer::OutputFormat>(format);
    sp<MediaMuxer> muxer = new MediaMuxer(fd, fileFormat);
    muxer->incStrong(clazz);
    return reinterpret_cast<jlong>(muxer.get());
}




static void android_media_MediaMuxer_start(JNIEnv *env, jclass /* clazz */,jlong nativeObject) {
    sp<MediaMuxer> muxer(reinterpret_cast<MediaMuxer *>(nativeObject));
    if (muxer == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", "Muxer was not set up correctly");
        return;
    }
    status_t err = muxer->start();

    if (err != OK) {
        jniThrowException(env, "java/lang/IllegalStateException", "Failed to start the muxer");
        return;
    }

}




static jint android_media_MediaMuxer_addTrack(JNIEnv *env, jclass /* clazz */, jlong nativeObject, jobjectArray keys,jobjectArray values) {
    sp<MediaMuxer> muxer(reinterpret_cast<MediaMuxer *>(nativeObject));
    if (muxer == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException","Muxer was not set up correctly");
        return -1;
    }

    sp<AMessage> trackformat;
    //将keys和values 添加到 trackformat中
    status_t err = ConvertKeyValueArraysToMessage(env, keys, values,&trackformat);
    if (err != OK) {
        jniThrowException(env, "java/lang/IllegalArgumentException","ConvertKeyValueArraysToMessage got an error");
        return err;
    }

    // Return negative value when errors happen in addTrack.
    jint trackIndex = muxer->addTrack(trackformat);

    if (trackIndex < 0) {
        jniThrowException(env, "java/lang/IllegalStateException","Failed to add the track to the muxer");
        return -1;
    }
    return trackIndex;
}


//	nativeWriteSampleData
static void android_media_MediaMuxer_writeSampleData(JNIEnv *env, jclass /* clazz */, jlong nativeObject, jint trackIndex,jobject byteBuf, jint offset, jint size, jlong timeUs, jint flags) {
    sp<MediaMuxer> muxer(reinterpret_cast<MediaMuxer *>(nativeObject));
    if (muxer == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException","Muxer was not set up correctly");
        return;
    }

    // Try to convert the incoming byteBuffer into ABuffer
    void *dst = env->GetDirectBufferAddress(byteBuf);

    jlong dstSize;
    jbyteArray byteArray = NULL;

    if (dst == NULL) {
        byteArray = (jbyteArray)env->CallObjectMethod(byteBuf, gFields.arrayID);

        if (byteArray == NULL) {
            jniThrowException(env, "java/lang/IllegalArgumentException","byteArray is null");
            return;
        }

        jboolean isCopy;
        dst = env->GetByteArrayElements(byteArray, &isCopy);

        dstSize = env->GetArrayLength(byteArray);
    } else {
        dstSize = env->GetDirectBufferCapacity(byteBuf);
    }

    if (dstSize < (offset + size)) {
        ALOGE("writeSampleData saw wrong dstSize %lld, size  %d, offset %d",(long long)dstSize, size, offset);
        if (byteArray != NULL) {
            env->ReleaseByteArrayElements(byteArray, (jbyte *)dst, 0);
        }
        jniThrowException(env, "java/lang/IllegalArgumentException","sample has a wrong size");
        return;
    }

    sp<ABuffer> buffer = new ABuffer((char *)dst + offset, size);

    status_t err = muxer->writeSampleData(buffer, trackIndex, timeUs, flags);

    if (byteArray != NULL) {
        env->ReleaseByteArrayElements(byteArray, (jbyte *)dst, 0);
    }

    if (err != OK) {
        jniThrowException(env, "java/lang/IllegalStateException","writeSampleData returned an error");
    }
    return;
}