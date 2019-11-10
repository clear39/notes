
//  @   frameworks/native/vulkan/vkjson/vkjson.cc

std::string VkJsonInstanceToJson(const VkJsonInstance& instance) {
  return VkTypeToJson(instance);
}


template <typename T> std::string VkTypeToJson(const T& t) {
  JsonWriterVisitor visitor;
  VisitForWrite(&visitor, t);
  return visitor.get_object().toStyledString();
}

template <typename Visitor, typename T>
inline void VisitForWrite(Visitor* visitor, const T& t) {
  Iterate(visitor, const_cast<T*>(&t));
}