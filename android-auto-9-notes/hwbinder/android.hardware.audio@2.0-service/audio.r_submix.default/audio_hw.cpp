
//  @   hardware/libhardware/modules/audio_remote_submix/audio_hw.cpp:89:#define ENABLE_RESAMPLING            1
#define ENABLE_RESAMPLING            1

//  @   hardware/libhardware/modules/audio_remote_submix/audio_hw.cpp:44:#define LOG_STREAMS_TO_FILES 0
#define LOG_STREAMS_TO_FILES 0

//  @   hardware/libhardware/modules/audio_remote_submix/audio_hw.cpp:85:#define ENABLE_LEGACY_INPUT_OPEN     1
#define ENABLE_LEGACY_INPUT_OPEN     1


//  @   hardware/libhardware/modules/audio_remote_submix/audio_hw.cpp:87:#define ENABLE_CHANNEL_CONVERSION    1
#define ENABLE_CHANNEL_CONVERSION    1


static struct hw_module_methods_t hal_module_methods = {
    /* open */ adev_open,
};


//  @   hardware/libhardware/modules/audio_remote_submix/audio_hw.cpp
struct audio_module HAL_MODULE_INFO_SYM = {
    /* common */ {
        /* tag */                HARDWARE_MODULE_TAG,       //  hardware/libhardware/include/hardware/hardware.h:34:#define HARDWARE_MODULE_TAG MAKE_TAG_CONSTANT('H', 'W', 'M', 'T')
        /* module_api_version */ AUDIO_MODULE_API_VERSION_0_1,  //  hardware/libhardware/include/hardware/audio.h:49:#define AUDIO_MODULE_API_VERSION_0_1 HARDWARE_MODULE_API_VERSION(0, 1)
        /* hal_api_version */    HARDWARE_HAL_API_VERSION,      //  hardware/libhardware/include/hardware/hardware.h:57:#define HARDWARE_HAL_API_VERSION HARDWARE_MAKE_API_VERSION(1, 0)
        /* id */                 AUDIO_HARDWARE_MODULE_ID,      //  hardware/libhardware/include/hardware/audio.h:38:#define AUDIO_HARDWARE_MODULE_ID "audio"
        /* name */               "Wifi Display audio HAL",
        /* author */             "The Android Open Source Project",
        /* methods */            &hal_module_methods,
        /* dso */                NULL,
        /* reserved */           { 0 },
    },
};


typedef struct route_config {
    struct submix_config config;
    /**
     * system/media/audio/include/system/audio.h:388:#define AUDIO_DEVICE_MAX_ADDRESS_LEN 32
    */
    char address[AUDIO_DEVICE_MAX_ADDRESS_LEN];
    // Pipe variables: they handle the ring buffer that "pipes" audio:
    //  - from the submix virtual audio output == what needs to be played
    //    remotely, seen as an output for AudioFlinger
    //  - to the virtual audio source == what is captured by the component
    //    which "records" the submix / virtual audio source, and handles it as needed.
    // A usecase example is one where the component capturing the audio is then sending it over
    // Wifi for presentation on a remote Wifi Display device (e.g. a dongle attached to a TV, or a
    // TV with Wifi Display capabilities), or to a wireless audio player.
    sp<MonoPipe> rsxSink;
    sp<MonoPipeReader> rsxSource;
    // Pointers to the current input and output stream instances.  rsxSink and rsxSource are
    // destroyed if both and input and output streams are destroyed.
    struct submix_stream_out *output;
    struct submix_stream_in *input;
#if ENABLE_RESAMPLING // 1
    // Buffer used as temporary storage for resampled data prior to returning data to the output
    // stream.
    int16_t resampler_buffer[DEFAULT_PIPE_SIZE_IN_FRAMES];
#endif // ENABLE_RESAMPLING
} route_config_t;

/**
 * hardware/libhardware/include/hardware/audio.h
*/



struct submix_audio_device {
    /**
     * hardware/libhardware/include/hardware/audio.h +623
    */
    struct audio_hw_device device;
    route_config_t routes[MAX_ROUTES];
    // Device lock, also used to protect access to submix_audio_device from the input and output
    // streams.
    pthread_mutex_t lock;
};


static int adev_open(const hw_module_t* module, const char* name,hw_device_t** device)
{
    ALOGI("adev_open(name=%s)", name);
    
    struct submix_audio_device *rsxadev;
    /**
     * hardware/libhardware/include/hardware/audio.h:43:#define AUDIO_HARDWARE_INTERFACE "audio_hw_if"
    */
    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) // "audio_hw_if"
        return -EINVAL;

    rsxadev = (submix_audio_device*) calloc(1, sizeof(struct submix_audio_device));
    if (!rsxadev)
        return -ENOMEM;

    rsxadev->device.common.tag = HARDWARE_DEVICE_TAG;
    rsxadev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    rsxadev->device.common.module = (struct hw_module_t *) module;
    rsxadev->device.common.close = adev_close;

    rsxadev->device.init_check = adev_init_check;
    rsxadev->device.set_voice_volume = adev_set_voice_volume;
    rsxadev->device.set_master_volume = adev_set_master_volume;
    rsxadev->device.get_master_volume = adev_get_master_volume;
    rsxadev->device.set_master_mute = adev_set_master_mute;
    rsxadev->device.get_master_mute = adev_get_master_mute;
    rsxadev->device.set_mode = adev_set_mode;
    rsxadev->device.set_mic_mute = adev_set_mic_mute;
    rsxadev->device.get_mic_mute = adev_get_mic_mute;
    rsxadev->device.set_parameters = adev_set_parameters;
    rsxadev->device.get_parameters = adev_get_parameters;
    rsxadev->device.get_input_buffer_size = adev_get_input_buffer_size;
    rsxadev->device.open_output_stream = adev_open_output_stream;
    rsxadev->device.close_output_stream = adev_close_output_stream;
    rsxadev->device.open_input_stream = adev_open_input_stream;
    rsxadev->device.close_input_stream = adev_close_input_stream;
    rsxadev->device.dump = adev_dump;

    /**
     * hardware/libhardware/modules/audio_remote_submix/audio_hw.cpp:144:#define MAX_ROUTES 10
    */
    for (int i=0 ; i < MAX_ROUTES ; i++) {
        memset(&rsxadev->routes[i], 0, sizeof(route_config));
        strcpy(rsxadev->routes[i].address, "");
    }
    /**
     * 这里 common 是 audio_hw_device 第一个成员，是可以指针强转成 audio_hw_device，
     * 同时也可以强制转换成 submix_audio_device 
    */
    *device = &rsxadev->device.common;

    return 0;
}


static int adev_close(hw_device_t *device)
{
    ALOGI("adev_close()");
    free(device);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags __unused,
                                  const char *address,
                                  audio_source_t source __unused)
{
    /**
     * 指针地址计算并且强转
    */
    struct submix_audio_device *rsxadev = audio_hw_device_get_submix_audio_device(dev);
    struct submix_stream_in *in;
    ALOGD("adev_open_input_stream(addr=%s)", address);
    (void)handle;
    (void)devices;

    *stream_in = NULL;

    // Do we already have a route for this address
    int route_idx = -1;

    pthread_mutex_lock(&rsxadev->lock);

    /**
     * 
    */
    status_t res = submix_get_route_idx_for_address_l(rsxadev, address, &route_idx);
    if (res != OK) {
        ALOGE("Error %d looking for address=%s in adev_open_input_stream", res, address);
        pthread_mutex_unlock(&rsxadev->lock);
        return res;
    }

    // Make sure it's possible to open the device given the current audio config.
    submix_sanitize_config(config, true);
    if (!submix_open_validate_l(rsxadev, route_idx, config, true)) {
        ALOGE("adev_open_input_stream(): Unable to open input stream.");
        pthread_mutex_unlock(&rsxadev->lock);
        return -EINVAL;
    }

#if ENABLE_LEGACY_INPUT_OPEN  // 1
    in = rsxadev->routes[route_idx].input;
    if (in) {
        in->ref_count++;
        sp<MonoPipe> sink = rsxadev->routes[route_idx].rsxSink;
        ALOG_ASSERT(sink != NULL);
        // If the sink has been shutdown, delete the pipe.
        if (sink != NULL) {
            if (sink->isShutdown()) {
                ALOGD(" Non-NULL shut down sink when opening input stream, releasing, refcount=%d",in->ref_count);
                submix_audio_device_release_pipe_l(rsxadev, in->route_handle);
            } else {
                ALOGD(" Non-NULL sink when opening input stream, refcount=%d", in->ref_count);
            }
        } else {
            ALOGE("NULL sink when opening input stream, refcount=%d", in->ref_count);
        }
    }
#else
    in = NULL;
#endif // ENABLE_LEGACY_INPUT_OPEN

    if (!in) {
        in = (struct submix_stream_in *)calloc(1, sizeof(struct submix_stream_in));
        if (!in) return -ENOMEM;
        in->ref_count = 1;

        // Initialize the function pointer tables (v-tables).
        in->stream.common.get_sample_rate = in_get_sample_rate;
        in->stream.common.set_sample_rate = in_set_sample_rate;
        in->stream.common.get_buffer_size = in_get_buffer_size;
        in->stream.common.get_channels = in_get_channels;
        in->stream.common.get_format = in_get_format;
        in->stream.common.set_format = in_set_format;
        in->stream.common.standby = in_standby;
        in->stream.common.dump = in_dump;
        in->stream.common.set_parameters = in_set_parameters;
        in->stream.common.get_parameters = in_get_parameters;
        in->stream.common.add_audio_effect = in_add_audio_effect;
        in->stream.common.remove_audio_effect = in_remove_audio_effect;
        in->stream.set_gain = in_set_gain;
        in->stream.read = in_read;
        in->stream.get_input_frames_lost = in_get_input_frames_lost;

        in->dev = rsxadev;
#if LOG_STREAMS_TO_FILES  //0
        in->log_fd = -1;
#endif
    }

    // Initialize the input stream.
    in->read_counter_frames = 0;
    in->input_standby = true;
    if (rsxadev->routes[route_idx].output != NULL) {
        in->output_standby_rec_thr = rsxadev->routes[route_idx].output->output_standby;
    } else {
        in->output_standby_rec_thr = true;
    }

    in->read_error_count = 0;
    // Initialize the pipe.
    ALOGV("adev_open_input_stream(): about to create pipe");
    submix_audio_device_create_pipe_l(rsxadev, config, DEFAULT_PIPE_SIZE_IN_FRAMES,DEFAULT_PIPE_PERIOD_COUNT, in, NULL, address, route_idx);
#if LOG_STREAMS_TO_FILES
    if (in->log_fd >= 0) close(in->log_fd);
    in->log_fd = open(LOG_STREAM_IN_FILENAME, O_CREAT | O_TRUNC | O_WRONLY,LOG_STREAM_FILE_PERMISSIONS);
    ALOGE_IF(in->log_fd < 0, "adev_open_input_stream(): log file open failed %s",strerror(errno));
    ALOGV("adev_open_input_stream(): log_fd = %d", in->log_fd);
#endif // LOG_STREAMS_TO_FILES
    // Return the input stream.
    *stream_in = &in->stream;

    pthread_mutex_unlock(&rsxadev->lock);
    return 0;
}


// Must be called with lock held on the submix_audio_device
static status_t submix_get_route_idx_for_address_l(const struct submix_audio_device * const rsxadev,
                                                 const char* address, /*in*/
                                                 int *idx /*out*/)
{
    // Do we already have a route for this address
    int route_idx = -1;
    int route_empty_idx = -1; // index of an empty route slot that can be used if needed
    for (int i=0 ; i < MAX_ROUTES ; i++) {
        if (strcmp(rsxadev->routes[i].address, "") == 0) {
            route_empty_idx = i;
        }
        if (strncmp(rsxadev->routes[i].address, address, AUDIO_DEVICE_MAX_ADDRESS_LEN) == 0) {
            route_idx = i;
            break;
        }
    }

    if ((route_idx == -1) && (route_empty_idx == -1)) {
        ALOGE("Cannot create new route for address %s, max number of routes reached", address);
        return -ENOMEM;
    }
    if (route_idx == -1) {
        route_idx = route_empty_idx;
    }
    *idx = route_idx;
    return OK;
}



static ssize_t in_read(struct audio_stream_in *stream, void* buffer,size_t bytes)
{
    struct submix_stream_in * const in = audio_stream_in_get_submix_stream_in(stream);
    struct submix_audio_device * const rsxadev = in->dev;
    /**
     * audio_stream_in_frame_size @ hardware/libhardware/include/hardware/audio.h +599
    */
    const size_t frame_size = audio_stream_in_frame_size(stream);
    const size_t frames_to_read = bytes / frame_size;

    SUBMIX_ALOGV("in_read bytes=%zu", bytes);
    pthread_mutex_lock(&rsxadev->lock);

    const bool output_standby = rsxadev->routes[in->route_handle].output == NULL ? true : rsxadev->routes[in->route_handle].output->output_standby;
    const bool output_standby_transition = (in->output_standby_rec_thr != output_standby);
    in->output_standby_rec_thr = output_standby;

    if (in->input_standby || output_standby_transition) {
        in->input_standby = false;
        // keep track of when we exit input standby (== first read == start "real recording")
        // or when we start recording silence, and reset projected time
        int rc = clock_gettime(CLOCK_MONOTONIC, &in->record_start_time);
        if (rc == 0) {
            in->read_counter_frames = 0;
        }
    }

    in->read_counter_frames += frames_to_read;
    size_t remaining_frames = frames_to_read;

    {
        // about to read from audio source
        sp<MonoPipeReader> source = rsxadev->routes[in->route_handle].rsxSource;
        if (source == NULL) {
            in->read_error_count++;// ok if it rolls over
            ALOGE_IF(in->read_error_count < MAX_READ_ERROR_LOGS,"no audio pipe yet we're trying to read! (not all errors will be logged)");
            pthread_mutex_unlock(&rsxadev->lock);
            usleep(frames_to_read * 1000000 / in_get_sample_rate(&stream->common));
            memset(buffer, 0, bytes);
            return bytes;
        }

        pthread_mutex_unlock(&rsxadev->lock);

        // read the data from the pipe (it's non blocking)
        int attempts = 0;
        char* buff = (char*)buffer;
#if ENABLE_CHANNEL_CONVERSION  // 1
        // Determine whether channel conversion is required.
        const uint32_t input_channels = audio_channel_count_from_in_mask(rsxadev->routes[in->route_handle].config.input_channel_mask);
        const uint32_t output_channels = audio_channel_count_from_out_mask(rsxadev->routes[in->route_handle].config.output_channel_mask);
        if (input_channels != output_channels) {
            SUBMIX_ALOGV("in_read(): %d output channels will be converted to %d " "input channels", output_channels, input_channels);
            // Only support 16-bit PCM channel conversion from mono to stereo or stereo to mono.
            ALOG_ASSERT(rsxadev->routes[in->route_handle].config.common.format == AUDIO_FORMAT_PCM_16_BIT);
            ALOG_ASSERT((input_channels == 1 && output_channels == 2) ||(input_channels == 2 && output_channels == 1));
        }
#endif // ENABLE_CHANNEL_CONVERSION

#if ENABLE_RESAMPLING // 1
        const uint32_t input_sample_rate = in_get_sample_rate(&stream->common);
        const uint32_t output_sample_rate = rsxadev->routes[in->route_handle].config.output_sample_rate;
        const size_t resampler_buffer_size_frames = sizeof(rsxadev->routes[in->route_handle].resampler_buffer) / sizeof(rsxadev->routes[in->route_handle].resampler_buffer[0]);
        float resampler_ratio = 1.0f;
        // Determine whether resampling is required.
        if (input_sample_rate != output_sample_rate) {
            resampler_ratio = (float)output_sample_rate / (float)input_sample_rate;
            // Only support 16-bit PCM mono resampling.
            // NOTE: Resampling is performed after the channel conversion step.
            ALOG_ASSERT(rsxadev->routes[in->route_handle].config.common.format == AUDIO_FORMAT_PCM_16_BIT);
            ALOG_ASSERT(audio_channel_count_from_in_mask(rsxadev->routes[in->route_handle].config.input_channel_mask) == 1);
        }
#endif // ENABLE_RESAMPLING

        while ((remaining_frames > 0) && (attempts < MAX_READ_ATTEMPTS)) {
            ssize_t frames_read = -1977;
            size_t read_frames = remaining_frames;
#if ENABLE_RESAMPLING  // 1
            char* const saved_buff = buff;
            if (resampler_ratio != 1.0f) {
                // Calculate the number of frames from the pipe that need to be read to generate
                // the data for the input stream read.
                const size_t frames_required_for_resampler = (size_t)((float)read_frames * (float)resampler_ratio);
                read_frames = min(frames_required_for_resampler, resampler_buffer_size_frames);
                // Read into the resampler buffer.
                buff = (char*)rsxadev->routes[in->route_handle].resampler_buffer;
            }
#endif // ENABLE_RESAMPLING
#if ENABLE_CHANNEL_CONVERSION  // 1
            if (output_channels == 1 && input_channels == 2) {
                // Need to read half the requested frames since the converted output
                // data will take twice the space (mono->stereo).
                read_frames /= 2;
            }
#endif // ENABLE_CHANNEL_CONVERSION

            SUBMIX_ALOGV("in_read(): frames available to read %zd", source->availableToRead());

            /**
             * 
            */
            frames_read = source->read(buff, read_frames);

            SUBMIX_ALOGV("in_read(): frames read %zd", frames_read);

#if ENABLE_CHANNEL_CONVERSION  // 1
            // Perform in-place channel conversion.
            // NOTE: In the following "input stream" refers to the data returned by this function
            // and "output stream" refers to the data read from the pipe.
            if (input_channels != output_channels && frames_read > 0) {
                int16_t *data = (int16_t*)buff;
                if (output_channels == 2 && input_channels == 1) {
                    // Offset into the output stream data in samples.
                    ssize_t output_stream_offset = 0;
                    for (ssize_t input_stream_frame = 0; input_stream_frame < frames_read;
                         input_stream_frame++, output_stream_offset += 2) {
                        // Average the content from both channels.
                        data[input_stream_frame] = ((int32_t)data[output_stream_offset] + (int32_t)data[output_stream_offset + 1]) / 2;
                    }
                } else if (output_channels == 1 && input_channels == 2) {
                    // Offset into the input stream data in samples.
                    ssize_t input_stream_offset = (frames_read - 1) * 2;
                    for (ssize_t output_stream_frame = frames_read - 1; output_stream_frame >= 0;
                         output_stream_frame--, input_stream_offset -= 2) {
                        const short sample = data[output_stream_frame];
                        data[input_stream_offset] = sample;
                        data[input_stream_offset + 1] = sample;
                    }
                }
            }
#endif // ENABLE_CHANNEL_CONVERSION

#if ENABLE_RESAMPLING   //  1
            if (resampler_ratio != 1.0f) {
                SUBMIX_ALOGV("in_read(): resampling %zd frames", frames_read);
                const int16_t * const data = (int16_t*)buff;
                int16_t * const resampled_buffer = (int16_t*)saved_buff;
                // Resample with *no* filtering - if the data from the ouptut stream was really
                // sampled at a different rate this will result in very nasty aliasing.
                const float output_stream_frames = (float)frames_read;
                size_t input_stream_frame = 0;
                for (float output_stream_frame = 0.0f;
                     output_stream_frame < output_stream_frames &&
                     input_stream_frame < remaining_frames;
                     output_stream_frame += resampler_ratio, input_stream_frame++) {
                    resampled_buffer[input_stream_frame] = data[(size_t)output_stream_frame];
                }
                ALOG_ASSERT(input_stream_frame <= (ssize_t)resampler_buffer_size_frames);
                SUBMIX_ALOGV("in_read(): resampler produced %zd frames", input_stream_frame);
                frames_read = input_stream_frame;
                buff = saved_buff;
            }
#endif // ENABLE_RESAMPLING

            if (frames_read > 0) {
#if LOG_STREAMS_TO_FILES  // 0
                if (in->log_fd >= 0) write(in->log_fd, buff, frames_read * frame_size);
#endif // LOG_STREAMS_TO_FILES

                remaining_frames -= frames_read;
                buff += frames_read * frame_size;
                SUBMIX_ALOGV("  in_read (att=%d) got %zd frames, remaining=%zu", attempts, frames_read, remaining_frames);
            } else {
                attempts++;
                SUBMIX_ALOGE("  in_read read returned %zd", frames_read);
                usleep(READ_ATTEMPT_SLEEP_MS * 1000);
            }
        }
        // done using the source
        pthread_mutex_lock(&rsxadev->lock);
        source.clear();
        pthread_mutex_unlock(&rsxadev->lock);
    }

    if (remaining_frames > 0) {
        const size_t remaining_bytes = remaining_frames * frame_size;
        SUBMIX_ALOGV("  clearing remaining_frames = %zu", remaining_frames);
        memset(((char*)buffer)+ bytes - remaining_bytes, 0, remaining_bytes);
    }

    // compute how much we need to sleep after reading the data by comparing the wall clock with
    //   the projected time at which we should return.
    struct timespec time_after_read;// wall clock after reading from the pipe
    struct timespec record_duration;// observed record duration
    int rc = clock_gettime(CLOCK_MONOTONIC, &time_after_read);
    const uint32_t sample_rate = in_get_sample_rate(&stream->common);
    if (rc == 0) {
        // for how long have we been recording?
        record_duration.tv_sec  = time_after_read.tv_sec - in->record_start_time.tv_sec;
        record_duration.tv_nsec = time_after_read.tv_nsec - in->record_start_time.tv_nsec;
        if (record_duration.tv_nsec < 0) {
            record_duration.tv_sec--;
            record_duration.tv_nsec += 1000000000;
        }

        // read_counter_frames contains the number of frames that have been read since the
        // beginning of recording (including this call): it's converted to usec and compared to
        // how long we've been recording for, which gives us how long we must wait to sync the
        // projected recording time, and the observed recording time.
        long projected_vs_observed_offset_us = ((int64_t)(in->read_counter_frames - (record_duration.tv_sec*sample_rate))) * 1000000 / sample_rate - (record_duration.tv_nsec / 1000);

        SUBMIX_ALOGV("  record duration %5lds %3ldms, will wait: %7ldus", record_duration.tv_sec, record_duration.tv_nsec/1000000, projected_vs_observed_offset_us);
        if (projected_vs_observed_offset_us > 0) {
            usleep(projected_vs_observed_offset_us);
        }
    }

    SUBMIX_ALOGV("in_read returns %zu", bytes);
    return bytes;

}

