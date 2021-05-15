/* codec private data */
struct aic3x_priv {                                                                                                                                                                                            
    struct snd_soc_codec *codec;
    struct regmap *regmap;
    struct regulator_bulk_data supplies[AIC3X_NUM_SUPPLIES];
    struct aic3x_disable_nb disable_nb[AIC3X_NUM_SUPPLIES];
    struct aic3x_setup_data *setup;
    unsigned int sysclk;
    unsigned int dai_fmt;
    unsigned int tdm_delay;
    unsigned int slot_width;
    struct list_head list;
    int master;
    int gpio_reset;
    int power;
#define AIC3X_MODEL_3X 0
#define AIC3X_MODEL_33 1
#define AIC3X_MODEL_3007 2
#define AIC3X_MODEL_3104 3
    u16 model;

    /* Selects the micbias voltage */
    enum aic3x_micbias_voltage micbias_vg;
    /* Output Common-Mode Voltage */
    u8 ocmv;
};
