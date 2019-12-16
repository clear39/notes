



struct ServiceContext {
    ServiceContext() {}

    std::vector<Module> mModules;

private:
    DISALLOW_COPY_AND_ASSIGN(ServiceContext);
};




struct Module {
    sp<V1_0::IBroadcastRadio> radioModule;
    HalRevision halRev;
    //  @   hardware/interfaces/broadcastradio/1.0/types.hal
    std::vector<hardware::broadcastradio::V1_0::BandConfig> bands;
};


//  @   hardware/interfaces/broadcastradio/1.0/types.hal

/* Radio band configuration. Describes a given band supported by the radio
 * module. The HAL can expose only one band per type with the the maximum range
 * supported and all options. The framework will derive the actual regions were
 * this module can operate and expose separate band configurations for
 * applications to chose from. */
struct BandConfig {
    Band type;
    bool antennaConnected;
    uint32_t lowerLimit;
    uint32_t upperLimit;
    vec<uint32_t> spacings;
    union Ext {
        FmBandConfig fm;
        AmBandConfig am;
    } ext;
};

/** value for field "type" of radio band described in struct radio_hal_band_config */
enum Band : uint32_t {
    /** Amplitude Modulation band: LW, MW, SW */
    AM     = 0,
    /** Frequency Modulation band: FM */
    FM     = 1,
    /** FM HD Radio / DRM (IBOC) */
    FM_HD  = 2,
    /** AM HD Radio / DRM (IBOC) */
    AM_HD  = 3,
};



/** Additional attributes for an FM band configuration */
struct FmBandConfig {
    /** deemphasis(降低重要性) variant(不同的) */
    Deemphasis deemphasis;
    /** stereo supported */
    bool       stereo;
    /** RDS variants supported */
    Rds        rds;
    /** Traffic Announcement supported */
    bool       ta;
    /** Alternate Frequency supported */
    bool       af;
    /** Emergency announcements supported */
    bool       ea;
};

/* FM deemphasis variant implemented.
 * A struct FmBandConfig can list one or more. */
enum Deemphasis : uint32_t {
    D50   = (1<<0),
    D75   = (1<<1),
};

/** Additional attributes for an AM band configuration */
struct AmBandConfig {
    /** Stereo supported */
    bool       stereo;
};