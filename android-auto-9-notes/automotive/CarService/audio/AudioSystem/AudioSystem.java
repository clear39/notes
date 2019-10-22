
//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/media/java/android/media/AudioSystem.java
public class AudioSystem{
    public static native int listAudioPorts(ArrayList<AudioPort> ports, int[] generation);
    public static native int listAudioPatches(ArrayList<AudioPatch> patches, int[] generation);
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


static jint
android_media_AudioSystem_listAudioPatches(JNIEnv *env, jobject clazz,jobject jPatches, jintArray jGeneration)
{
    ALOGV("listAudioPatches");
    if (jPatches == NULL) {
        ALOGE("listAudioPatches NULL AudioPatch ArrayList");
        return (jint)AUDIO_JAVA_BAD_VALUE;
    }
    if (!env->IsInstanceOf(jPatches, gArrayListClass)) {
        ALOGE("listAudioPatches not an arraylist");
        return (jint)AUDIO_JAVA_BAD_VALUE;
    }

    if (jGeneration == NULL || env->GetArrayLength(jGeneration) != 1) {
        return (jint)AUDIO_JAVA_BAD_VALUE;
    }

    status_t status;
    unsigned int generation1;
    unsigned int generation;
    unsigned int numPatches;
    jint *nGeneration;
    struct audio_patch *nPatches = NULL;
    jobjectArray jSources = NULL;
    jobject jSource = NULL;
    jobjectArray jSinks = NULL;
    jobject jSink = NULL;
    jobject jPatch = NULL;
    int attempts = MAX_PORT_GENERATION_SYNC_ATTEMPTS;
    jint jStatus;

    // get the patch count and all the patches until they both return the same generation
    do {
        if (attempts-- < 0) {
            status = TIMED_OUT;
            break;
        }

        numPatches = 0;
        status = AudioSystem::listAudioPatches(&numPatches,NULL,&generation1);
        if (status != NO_ERROR) {
            ALOGE_IF(status != NO_ERROR, "listAudioPatches AudioSystem::listAudioPatches error %d",status);
            break;
        }
        if (numPatches == 0) {
            jStatus = (jint)AUDIO_JAVA_SUCCESS;
            goto exit;
        }

        nPatches = (struct audio_patch *)realloc(nPatches, numPatches * sizeof(struct audio_patch));

        status = AudioSystem::listAudioPatches(&numPatches,nPatches,&generation);
        ALOGV("listAudioPatches AudioSystem::listAudioPatches numPatches %d generation %d generation1 %d",
              numPatches, generation, generation1);

    } while (generation1 != generation && status == NO_ERROR);

    jStatus = nativeToJavaStatus(status);
    if (jStatus != AUDIO_JAVA_SUCCESS) {
        goto exit;
    }

    for (size_t i = 0; i < numPatches; i++) {
        jobject patchHandle = env->NewObject(gAudioHandleClass, gAudioHandleCstor,nPatches[i].id);
        if (patchHandle == NULL) {
            jStatus = AUDIO_JAVA_ERROR;
            goto exit;
        }
        ALOGV("listAudioPatches patch %zu num_sources %d num_sinks %d",
              i, nPatches[i].num_sources, nPatches[i].num_sinks);

        env->SetIntField(patchHandle, gAudioHandleFields.mId, nPatches[i].id);

        // load sources
        jSources = env->NewObjectArray(nPatches[i].num_sources,gAudioPortConfigClass, NULL);
        if (jSources == NULL) {
            jStatus = AUDIO_JAVA_ERROR;
            goto exit;
        }

        for (size_t j = 0; j < nPatches[i].num_sources; j++) {
            jStatus = convertAudioPortConfigFromNative(env,NULL,&jSource,&nPatches[i].sources[j]);
            if (jStatus != AUDIO_JAVA_SUCCESS) {
                goto exit;
            }
            env->SetObjectArrayElement(jSources, j, jSource);
            env->DeleteLocalRef(jSource);
            jSource = NULL;
            ALOGV("listAudioPatches patch %zu source %zu is a %s handle %d",
                  i, j,
                  nPatches[i].sources[j].type == AUDIO_PORT_TYPE_DEVICE ? "device" : "mix",
                  nPatches[i].sources[j].id);
        }
        // load sinks
        jSinks = env->NewObjectArray(nPatches[i].num_sinks,gAudioPortConfigClass, NULL);
        if (jSinks == NULL) {
            jStatus = AUDIO_JAVA_ERROR;
            goto exit;
        }

        for (size_t j = 0; j < nPatches[i].num_sinks; j++) {
            jStatus = convertAudioPortConfigFromNative(env,
                                                      NULL,
                                                      &jSink,
                                                      &nPatches[i].sinks[j]);

            if (jStatus != AUDIO_JAVA_SUCCESS) {
                goto exit;
            }
            env->SetObjectArrayElement(jSinks, j, jSink);
            env->DeleteLocalRef(jSink);
            jSink = NULL;
            ALOGV("listAudioPatches patch %zu sink %zu is a %s handle %d",
                  i, j,
                  nPatches[i].sinks[j].type == AUDIO_PORT_TYPE_DEVICE ? "device" : "mix",
                  nPatches[i].sinks[j].id);
        }

        jPatch = env->NewObject(gAudioPatchClass, gAudioPatchCstor,patchHandle, jSources, jSinks);
        env->DeleteLocalRef(jSources);
        jSources = NULL;
        env->DeleteLocalRef(jSinks);
        jSinks = NULL;
        if (jPatch == NULL) {
            jStatus = AUDIO_JAVA_ERROR;
            goto exit;
        }
        env->CallBooleanMethod(jPatches, gArrayListMethods.add, jPatch);
        env->DeleteLocalRef(jPatch);
        jPatch = NULL;
    }

exit:

    nGeneration = env->GetIntArrayElements(jGeneration, NULL);
    if (nGeneration == NULL) {
        jStatus = AUDIO_JAVA_ERROR;
    } else {
        nGeneration[0] = generation1;
        env->ReleaseIntArrayElements(jGeneration, nGeneration, 0);
    }

    if (jSources != NULL) {
        env->DeleteLocalRef(jSources);
    }
    if (jSource != NULL) {
        env->DeleteLocalRef(jSource);
    }
    if (jSinks != NULL) {
        env->DeleteLocalRef(jSinks);
    }
    if (jSink != NULL) {
        env->DeleteLocalRef(jSink);
    }
    if (jPatch != NULL) {
        env->DeleteLocalRef(jPatch);
    }
    free(nPatches);
    return jStatus;
}