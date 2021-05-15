
//  @   include/sound/soc.h

struct snd_soc_dai_link {
    /* config - must be set by machine driver */
    const char *name;           /* Codec name */
    const char *stream_name;        /* Stream name */
    /*
     * You MAY specify the link's CPU-side device, either by device name,
     * or by DT/OF node, but not both. If this information is omitted,
     * the CPU-side DAI is matched using .cpu_dai_name only, which hence
     * must be globally unique. These fields are currently typically used
     * only for codec to codec links, or systems using device tree.
     */
    const char *cpu_name;
    struct device_node *cpu_of_node;
    /*
     * You MAY specify the DAI name of the CPU DAI. If this information is
     * omitted, the CPU-side DAI is matched using .cpu_name/.cpu_of_node
     * only, which only works well when that device exposes a single DAI.
     */
    const char *cpu_dai_name;
    /*
     * You MUST specify the link's codec, either by device name, or by
     * DT/OF node, but not both.
     */
    const char *codec_name;
    struct device_node *codec_of_node;
    /* You MUST specify the DAI name within the codec */
    const char *codec_dai_name;

    struct snd_soc_dai_link_component *codecs;
    unsigned int num_codecs;

    /*
     * You MAY specify the link's platform/PCM/DMA driver, either by
     * device name, or by DT/OF node, but not both. Some forms of link
     * do not need a platform.
     */
    const char *platform_name;
    struct device_node *platform_of_node;
    int id; /* optional ID for machine driver link identification */

    const struct snd_soc_pcm_stream *params;
    unsigned int num_params;

    unsigned int dai_fmt;           /* format to set on init */

    enum snd_soc_dpcm_trigger trigger[2]; /* trigger type for DPCM */

    /* codec/machine specific init - e.g. add machine controls */
    int (*init)(struct snd_soc_pcm_runtime *rtd);

    /* optional hw_params re-writing for BE and FE sync */
    int (*be_hw_params_fixup)(struct snd_soc_pcm_runtime *rtd, struct snd_pcm_hw_params *params);

    /* machine stream operations */
    const struct snd_soc_ops *ops;
    const struct snd_soc_compr_ops *compr_ops;

    /* For unidirectional dai links */
    bool playback_only;
    bool capture_only;

    /* Mark this pcm with non atomic ops */
    bool nonatomic;

    /* Keep DAI active over suspend */
    unsigned int ignore_suspend:1;

    /* Symmetry requirements */
    unsigned int symmetric_rates:1;
    unsigned int symmetric_channels:1;
    unsigned int symmetric_samplebits:1;

    /* Do not create a PCM for this DAI link (Backend link) */
    unsigned int no_pcm:1;

    /* This DAI link can route to other DAI links at runtime (Frontend)*/
    unsigned int dynamic:1;

    /* DPCM capture and Playback support */
    unsigned int dpcm_capture:1;
    unsigned int dpcm_playback:1;

    /* DPCM used FE & BE merged format */
    unsigned int dpcm_merged_format:1;

    /* pmdown_time is ignored at stop */
    unsigned int ignore_pmdown_time:1;

    struct list_head list; /* DAI link list of the soc card */
    struct snd_soc_dobj dobj; /* For topology */
};
