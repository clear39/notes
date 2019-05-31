//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/vehicle/2.0/default/impl/vhal_v2_0/DefaultConfig.h

struct ConfigDeclaration {
    VehiclePropConfig config;  //   ./vehicle/2.0/types.hal:2567:struct VehiclePropConfig {

    /* This value will be used as an initial value for the property. If this field is specified for
     * property that supports multiple areas then it will be used for all areas unless particular
     * area is overridden in initialAreaValue field. */
    VehiclePropValue::RawValue initialValue;
    /* Use initialAreaValues if it is necessary to specify different values per each area. */
    std::map<int32_t, VehiclePropValue::RawValue> initialAreaValues;
};

const ConfigDeclaration kVehicleProperties[]{
    {.config =
         {
             .prop = toInt(VehicleProperty::INFO_FUEL_CAPACITY),
             .access = VehiclePropertyAccess::READ,
             .changeMode = VehiclePropertyChangeMode::STATIC,
         },
     .initialValue = {.floatValues = {15000}}},

    ......
};


/**
 * Property config defines the capabilities of it. User of the API
 * must first get the property config to understand the output from get()
 * commands and also to ensure that set() or events commands are in sync with
 * the expected output.
 */
enum VehiclePropertyAccess : int32_t {
    NONE = 0x00,

    READ = 0x01,
    WRITE = 0x02,
    READ_WRITE = 0x03,
};


/**
 * This describes how value of property can change.
 */
enum VehiclePropertyChangeMode : int32_t {
    /**
     * Property of this type must never be changed. Subscription is not supported
     * for these properties.
     */
    STATIC = 0x00,

    /**
     * Properties of this type must report when there is a change.
     * IVehicle#get call must return the current value.
     * Set operation for this property is assumed to be asynchronous. When the
     * property is read (using IVehicle#get) after IVehicle#set, it may still
     * return old value until underlying H/W backing this property has actually
     * changed the state. Once state is changed, the property must dispatch
     * changed value as event.
     */
    ON_CHANGE = 0x01,

    /**
     * Properties of this type change continuously(连续不断地) and require a fixed rate of
     * sampling to retrieve the data.  Implementers may choose to send extra
     * notifications on significant value changes.
     */
    CONTINUOUS = 0x02,
};

struct VehicleAreaConfig {
    /**
     * Area id is ignored for VehiclePropertyGroup:GLOBAL properties.
     */
    int32_t areaId;

    int32_t minInt32Value;
    int32_t maxInt32Value;

    int64_t minInt64Value;
    int64_t maxInt64Value;

    float minFloatValue;
    float maxFloatValue;
};


struct VehiclePropConfig {
    /** Property identifier */
    int32_t prop;  // 低16，31~28表示 VehiclePropertyGroup（那个组） 类型；27~20位表示 VehiclePropertyType（数据类型）；19~16位表示 VehicleArea

    /**
     * Defines if the property is read or write or both.
     */
    VehiclePropertyAccess access; // 表示可读可写

    /**
     * Defines the change mode of the property.
     */
    VehiclePropertyChangeMode changeMode; //表示属性为静态（不可更改），可以更改通知监听回调

    /**
     * Contains per-area configuration.
     */
    vec<VehicleAreaConfig> areaConfigs;

    /** Contains additional configuration parameters */
    vec<int32_t> configArray;

    /**
     * Some properties may require additional information passed over this
     * string. Most properties do not need to set this.
     */
    string configString;

    /**
     * Min sample rate in Hz.
     * Must be defined for VehiclePropertyChangeMode::CONTINUOUS
     */
    float minSampleRate;

    /**
     * Must be defined for VehiclePropertyChangeMode::CONTINUOUS
     * Max sample rate in Hz.
     */
    float maxSampleRate;
};



/**
 * Encapsulates the property name and the associated value. It
 * is used across various API calls to set values, get values or to register for
 * events.
 */
struct VehiclePropValue {
    /** Time is elapsed nanoseconds since boot */
    int64_t timestamp;

    /**
     * Area type(s) for non-global property it must be one of the value from
     * VehicleArea* enums or 0 for global properties.
     */
    int32_t areaId;

    /** Property identifier */
    int32_t prop;

    /** Status of the property */
    VehiclePropertyStatus status;

    /**
     * Contains value for a single property. Depending on property data type of
     * this property (VehiclePropetyType) one field of this structure must be filled in.
     */
    struct RawValue {
        /**
         * This is used for properties of types VehiclePropertyType#INT
         * and VehiclePropertyType#INT_VEC
         */
        vec<int32_t> int32Values;

        /**
         * This is used for properties of types VehiclePropertyType#FLOAT
         * and VehiclePropertyType#FLOAT_VEC
         */
        vec<float> floatValues;

        /** This is used for properties of type VehiclePropertyType#INT64 */
        vec<int64_t> int64Values;

        /** This is used for properties of type VehiclePropertyType#BYTES */
        vec<uint8_t> bytes;

        /** This is used for properties of type VehiclePropertyType#STRING */
        string stringValue;
    };

    RawValue value;
};

/**
 * Property status is a dynamic value that may change based on the vehicle state.
 */
enum VehiclePropertyStatus : int32_t {
    /** Property is available and behaving normally */
    AVAILABLE   = 0x00,
    /**
     * A property in this state is not available for reading and writing.  This
     * is a transient state that depends on the availability of the underlying
     * implementation (e.g. hardware or driver). It MUST NOT be used to
     * represent features that this vehicle is always incapable of.  A get() of
     * a property in this state MAY return an undefined value, but MUST
     * correctly describe its status as UNAVAILABLE A set() of a property in
     * this state MAY return NOT_AVAILABLE. The HAL implementation MUST ignore
     * the value of the status field when writing a property value coming from
     * Android.
     */
    UNAVAILABLE = 0x01,
    /** There is an error with this property. */
    ERROR       = 0x02,
};