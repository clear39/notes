/**
 * @    frameworks/native/vulkan/vkjson/vkjson_instance.cc 
 * 
 * 
*/

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

  std::map<VkPhysicalDevice, uint32_t> device_map;
  const uint32_t sz = devices.size();
  instance.devices.reserve(sz);
  for (uint32_t i = 0; i < sz; ++i) {
    device_map.insert(std::make_pair(devices[i], i));
    instance.devices.emplace_back(VkJsonGetDevice(vkinstance, devices[i],
                                                  instance_extensions.size(),
                                                  instance_extensions.data()));
  }

  PFN_vkEnumerateInstanceVersion vkpEnumerateInstanceVersion =
      reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
          vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
  if (!vkpEnumerateInstanceVersion) {
    instance.api_version = VK_API_VERSION_1_0;
  } else {
    result = (*vkpEnumerateInstanceVersion)(&instance.api_version);
    if (result != VK_SUCCESS) {
      vkDestroyInstance(vkinstance, nullptr);
      return VkJsonInstance();
    }
  }

  PFN_vkEnumeratePhysicalDeviceGroups vkpEnumeratePhysicalDeviceGroups =
      reinterpret_cast<PFN_vkEnumeratePhysicalDeviceGroups>(
          vkGetInstanceProcAddr(vkinstance, "vkEnumeratePhysicalDeviceGroups"));
  if (vkpEnumeratePhysicalDeviceGroups) {
    count = 0;
    result = (*vkpEnumeratePhysicalDeviceGroups)(vkinstance, &count, nullptr);
    if (result != VK_SUCCESS) {
      vkDestroyInstance(vkinstance, nullptr);
      return VkJsonInstance();
    }

    VkJsonDeviceGroup device_group;
    std::vector<VkPhysicalDeviceGroupProperties> group_properties;
    group_properties.resize(count);
    result = (*vkpEnumeratePhysicalDeviceGroups)(vkinstance, &count,
                                                 group_properties.data());
    if (result != VK_SUCCESS) {
      vkDestroyInstance(vkinstance, nullptr);
      return VkJsonInstance();
    }
    for (auto properties : group_properties) {
      device_group.properties = properties;
      for (uint32_t i = 0; i < properties.physicalDeviceCount; ++i) {
        device_group.device_inds.push_back(
            device_map[properties.physicalDevices[i]]);
      }
      instance.device_groups.push_back(device_group);
    }
  }

  vkDestroyInstance(vkinstance, nullptr);
  return instance;
}