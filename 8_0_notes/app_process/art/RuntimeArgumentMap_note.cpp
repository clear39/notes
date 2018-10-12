 //  art/runtime/base/variant_map.h


// Allocate a unique counter value each time it's called.
struct VariantMapKeyCounterAllocator {
  static size_t AllocateCounter() {
    static size_t counter = 0;
    counter++;

    return counter;
  }
};


// Type-erased version of VariantMapKey<T>
struct VariantMapKeyRaw {
  // TODO: this may need to call a virtual function to support string comparisons
  bool operator<(const VariantMapKeyRaw& other) const {
    return key_counter_ < other.key_counter_;
  }

  // The following functions need to be virtual since we don't know the compile-time type anymore:

  // Clone the key, creating a copy of the contents.
  virtual VariantMapKeyRaw* Clone() const = 0;

  // Delete a value whose runtime type is that of the non-erased key's TValue.
  virtual void ValueDelete(void* value) const = 0;

  // Clone a value whose runtime type is that of the non-erased key's TValue.
  virtual void* ValueClone(void* value) const = 0;

  // Compare one key to another (same as operator<).
  virtual bool Compare(const VariantMapKeyRaw* other) const {
    if (other == nullptr) {
      return false;
    }
    return key_counter_ < other->key_counter_;
  }

  virtual ~VariantMapKeyRaw() {}

 protected:
  VariantMapKeyRaw(): key_counter_(VariantMapKeyCounterAllocator::AllocateCounter()) {}
  // explicit VariantMapKeyRaw(size_t counter)
  //     : key_counter_(counter) {}

  size_t GetCounter() const {
    return key_counter_;
  }

 protected:
  // Avoid the object slicing problem; use Clone() instead.
  VariantMapKeyRaw(const VariantMapKeyRaw&) = default;
  VariantMapKeyRaw(VariantMapKeyRaw&&) = default;

 private:
  size_t key_counter_;  // Runtime type ID. Unique each time a new type is reified.
};





template <typename TValue>
struct VariantMapKey : detail::VariantMapKeyRaw {
  // Instantiate a default value for this key. If an explicit default value was provided
  // then that is used. Otherwise, the default value for the type TValue{} is returned.
  TValue CreateDefaultValue() const {
    if (default_value_ == nullptr) {
      return TValue{};  // NOLINT [readability/braces] [4]
    } else {
      return TValue(*default_value_);
    }
  }

 protected:
  // explicit VariantMapKey(size_t counter) : detail::VariantMapKeyRaw(counter) {}
  explicit VariantMapKey(const TValue& default_value): default_value_(std::make_shared<TValue>(default_value)) {}
  explicit VariantMapKey(TValue&& default_value): default_value_(std::make_shared<TValue>(default_value)) {}
  VariantMapKey() {}
  virtual ~VariantMapKey() {}

 private:
  virtual VariantMapKeyRaw* Clone() const {
    return new VariantMapKey<TValue>(*this);
  }

  virtual void* ValueClone(void* value) const {
    if (value == nullptr) {
      return nullptr;
    }

    TValue* strong_value = reinterpret_cast<TValue*>(value);
    return new TValue(*strong_value);
  }

  virtual void ValueDelete(void* value) const {
    if (value == nullptr) {
      return;
    }

    // Smartly invoke the proper delete/delete[]/etc
    const std::default_delete<TValue> deleter = std::default_delete<TValue>();
    deleter(reinterpret_cast<TValue*>(value));
  }

  VariantMapKey(const VariantMapKey&) = default;
  VariantMapKey(VariantMapKey&&) = default;

  template <typename Base, template <typename TV> class TKey> friend struct VariantMap;

  // Store a prototype of the key's default value, for usage with VariantMap::GetOrDefault
  std::shared_ptr<TValue> default_value_;
};








// Define a key that is usable with a RuntimeArgumentMap.
// This key will *not* work with other subtypes of VariantMap.
template <typename TValue>
struct RuntimeArgumentMapKey : VariantMapKey<TValue> {
  RuntimeArgumentMapKey() {}
  explicit RuntimeArgumentMapKey(TValue default_value)
    : VariantMapKey<TValue>(std::move(default_value)) {}
  // Don't ODR-use constexpr default values, which means that Struct::Fields
  // that are declared 'static constexpr T Name = Value' don't need to have a matching definition.
};