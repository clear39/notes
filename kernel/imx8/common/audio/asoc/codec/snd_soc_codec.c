

//  @   include/sound/soc.h

/* SoC Audio Codec device */
struct snd_soc_codec {                                                                                                                                                                                         
    struct device *dev;
    const struct snd_soc_codec_driver *driver;

    struct list_head list;
    struct list_head card_list;

    /* runtime */
    unsigned int cache_bypass:1; /* Suppress access to the cache */
    unsigned int suspended:1; /* Codec is in suspend PM state */
    unsigned int cache_init:1; /* codec cache has been initialized */

    /* codec IO */
    void *control_data; /* codec control (i2c/3wire) data */
    hw_write_t hw_write;
    void *reg_cache;

    /* component */
    struct snd_soc_component component;

#ifdef CONFIG_DEBUG_FS
    struct dentry *debugfs_reg;
#endif
};
