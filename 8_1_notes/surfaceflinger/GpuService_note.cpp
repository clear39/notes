

status_t GpuService::shellCommand(int /*in*/, int out, int err,Vector<String16>& args)
{
    ALOGV("GpuService::shellCommand");
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
    std::string json = VkJsonInstanceToJson(VkJsonGetInstance());
    fwrite(json.data(), 1, json.size(), out);
    fputc('\n', out);
}

//@external/vulkan-validation-layers/libs/vkjson/vkjson_instance.cc
VkJsonInstance VkJsonGetInstance() {
  VkJsonInstance instance;
  VkResult result;
  uint32_t count;

  count = 0;
  result = vkEnumerateInstanceLayerProperties(&count, nullptr);
  if (result != VK_SUCCESS)
    return VkJsonInstance();
  if (count > 0) {
    std::vector<VkLayerProperties> layers(count);
    result = vkEnumerateInstanceLayerProperties(&count, layers.data());
    if (result != VK_SUCCESS)
      return VkJsonInstance();
    instance.layers.reserve(count);
    for (auto& layer : layers) {
      instance.layers.push_back(VkJsonLayer{layer, std::vector<VkExtensionProperties>()});
      if (!EnumerateExtensions(layer.layerName,&instance.layers.back().extensions))
        return VkJsonInstance();
    }
  }

  if (!EnumerateExtensions(nullptr, &instance.extensions))
    return VkJsonInstance();

  std::vector<const char*> instance_extensions;
  for (const auto extension : kSupportedInstanceExtensions) {
    if (HasExtension(extension, instance.extensions))
      instance_extensions.push_back(extension);
  }

  const VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                      nullptr,
                                      "vkjson_info",
                                      1,
                                      "",
                                      0,
                                      VK_API_VERSION_1_0};
  VkInstanceCreateInfo instance_info = {
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      nullptr,
      0,
      &app_info,
      0,
      nullptr,
      static_cast<uint32_t>(instance_extensions.size()),
      instance_extensions.data()};
  VkInstance vkinstance;
  result = vkCreateInstance(&instance_info, nullptr, &vkinstance);
  if (result != VK_SUCCESS)
    return VkJsonInstance();

  count = 0;
  result = vkEnumeratePhysicalDevices(vkinstance, &count, nullptr);
  if (result != VK_SUCCESS) {
    vkDestroyInstance(vkinstance, nullptr);
    return VkJsonInstance();
  }
  std::vector<VkPhysicalDevice> devices(count, VK_NULL_HANDLE);
  result = vkEnumeratePhysicalDevices(vkinstance, &count, devices.data());
  if (result != VK_SUCCESS) {
    vkDestroyInstance(vkinstance, nullptr);
    return VkJsonInstance();
  }

  instance.devices.reserve(devices.size());
  for (auto device : devices)
    instance.devices.emplace_back(VkJsonGetDevice(vkinstance, device,instance_extensions.size(),instance_extensions.data()));
  vkDestroyInstance(vkinstance, nullptr);
  return instance;
}



//		@external/vulkan-validation-layers/libs/vkjson/vkjson.cc
std::string VkJsonInstanceToJson(const VkJsonInstance& instance) {
  return VkTypeToJson(instance);
}

template <typename T> 
std::string VkTypeToJson(const T& t) {
  JsonWriterVisitor visitor;
  VisitForWrite(&visitor, t);

  char* output = cJSON_Print(visitor.get_object());
  std::string result(output);
  free(output);
  return result;
}


template <typename Visitor, typename T>
inline void VisitForWrite(Visitor* visitor, const T& t) {
  Iterate(visitor, const_cast<T*>(&t));
}


template <typename Visitor>
inline bool Iterate(Visitor* visitor, VkJsonInstance* instance) {
  return visitor->Visit("layers", &instance->layers) &&
         visitor->Visit("extensions", &instance->extensions) &&
         visitor->Visit("devices", &instance->devices);
}
