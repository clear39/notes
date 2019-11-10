

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/native/services/surfaceflinger/GpuService.cpp

const char* const GpuService::SERVICE_NAME = "gpu";

GpuService::GpuService() {

}


status_t GpuService::shellCommand(int /*in*/, int out, int err,Vector<String16>& args)
{
    ALOGV("GpuService::shellCommand");
    /**
     * 打印每个参数
    */
    for (size_t i = 0, n = args.size(); i < n; i++)
        ALOGV("  arg[%zu]: '%s'", i, String8(args[i]).string());

    if (args.size() >= 1) {
        if (args[0] == String16("vkjson"))
            return cmd_vkjson(out, err);
        if (args[0] == String16("help"))
            return cmd_help(out);
    }
    // no command, or unrecognized command
    cmd_help(err);
    return BAD_VALUE;
}



status_t cmd_help(int out) {
    FILE* outs = fdopen(out, "w");
    if (!outs) {
        ALOGE("vkjson: failed to create out stream: %s (%d)", strerror(errno),errno);
        return BAD_VALUE;
    }
    fprintf(outs,
        "GPU Service commands:\n"
        "  vkjson   dump Vulkan properties as JSON\n");
    fclose(outs);
    return NO_ERROR;
}

status_t cmd_vkjson(int out, int /*err*/) {
    FILE* outs = fdopen(out, "w");
    if (!outs) {
        int errnum = errno;
        ALOGE("vkjson: failed to create output stream: %s", strerror(errnum));
        return -errnum;
    }
    vkjsonPrint(outs);
    fclose(outs);
    return NO_ERROR;
}

void vkjsonPrint(FILE* out) {
    /**
     * VkJsonGetInstance @  frameworks/native/vulkan/vkjson/vkjson_instance.cc 
     * VkJsonInstanceToJson @   frameworks/native/vulkan/vkjson/vkjson.cc
    */
    std::string json = VkJsonInstanceToJson(VkJsonGetInstance());
    fwrite(json.data(), 1, json.size(), out);
    fputc('\n', out);
}

