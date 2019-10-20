
//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/media/java/android/media/AudioSystem.java
public class AudioSystem{
    public static native int listAudioPorts(ArrayList<AudioPort> ports, int[] generation);
}








/***
 * 
 * /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/core/jni/android_media_AudioSystem.cpp
 * 
 */
static jint
android_media_AudioSystem_listAudioPorts(JNIEnv *env, jobject clazz,jobject jPorts, jintArray jGeneration)
{
    ALOGV("listAudioPorts");

    if (jPorts == NULL) {
        ALOGE("listAudioPorts NULL AudioPort ArrayList");
        return (jint)AUDIO_JAVA_BAD_VALUE;
    }
    if (!env->IsInstanceOf(jPorts, gArrayListClass)) {
        ALOGE("listAudioPorts not an arraylist");
        return (jint)AUDIO_JAVA_BAD_VALUE;
    }

    if (jGeneration == NULL || env->GetArrayLength(jGeneration) != 1) {
        return (jint)AUDIO_JAVA_BAD_VALUE;
    }

    status_t status;
    unsigned int generation1;
    unsigned int generation;
    unsigned int numPorts;
    jint *nGeneration;
    struct audio_port *nPorts = NULL;
    int attempts = MAX_PORT_GENERATION_SYNC_ATTEMPTS;
    jint jStatus;

    // get the port count and all the ports until they both return the same generation
    do {
        if (attempts-- < 0) {
            status = TIMED_OUT;
            break;
        }

        numPorts = 0;
        /***
         * 
         *  AudioSystem::listAudioPorts --> AudioPolicyService::listAudioPorts --> AudioPolicyManager::listAudioPorts
         */
        status = AudioSystem::listAudioPorts(AUDIO_PORT_ROLE_NONE,AUDIO_PORT_TYPE_NONE,&numPorts,NULL,&generation1);
        if (status != NO_ERROR) {
            ALOGE_IF(status != NO_ERROR, "AudioSystem::listAudioPorts error %d", status);
            break;
        }
        if (numPorts == 0) {
            jStatus = (jint)AUDIO_JAVA_SUCCESS;
            goto exit;
        }
        nPorts = (struct audio_port *)realloc(nPorts, numPorts * sizeof(struct audio_port));

        status = AudioSystem::listAudioPorts(AUDIO_PORT_ROLE_NONE,AUDIO_PORT_TYPE_NONE,&numPorts,nPorts,&generation);
        ALOGV("listAudioPorts AudioSystem::listAudioPorts numPorts %d generation %d generation1 %d",numPorts, generation, generation1);
    } while (generation1 != generation && status == NO_ERROR);

    jStatus = nativeToJavaStatus(status);
    if (jStatus != AUDIO_JAVA_SUCCESS) {
        goto exit;
    }

    for (size_t i = 0; i < numPorts; i++) {
        jobject jAudioPort;
        jStatus = convertAudioPortFromNative(env, &jAudioPort, &nPorts[i]);
        if (jStatus != AUDIO_JAVA_SUCCESS) {
            goto exit;
        }
        env->CallBooleanMethod(jPorts, gArrayListMethods.add, jAudioPort);
    }

exit:
    nGeneration = env->GetIntArrayElements(jGeneration, NULL);
    if (nGeneration == NULL) {
        jStatus = (jint)AUDIO_JAVA_ERROR;
    } else {
        nGeneration[0] = generation1;
        env->ReleaseIntArrayElements(jGeneration, nGeneration, 0);
    }
    free(nPorts);
    return jStatus;
}