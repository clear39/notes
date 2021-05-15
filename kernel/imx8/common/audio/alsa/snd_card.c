
// @    include/sound/core.h

/* main structure for soundcard */                                                                                                                                                                             
struct snd_card {
    int number;         /* number of soundcard (index to snd_cards) */

    char id[16];            /* id string of this card */
    char driver[16];        /* driver name */
    char shortname[32];     /* short name of this soundcard */
    char longname[80];      /* name of this soundcard */
    char irq_descr[32];     /* Interrupt description */
    char mixername[80];     /* mixer name */
    char components[128];       /* card components delimited with space */
    struct module *module;      /* top-level module */

    void *private_data;     /* private data for soundcard */
    void (*private_free) (struct snd_card *card); /* callback for freeing of private data */
    struct list_head devices;   /* devices */

    struct device ctl_dev;      /* control device */
    unsigned int last_numid;    /* last used numeric ID */
    struct rw_semaphore controls_rwsem; /* controls list lock */
    rwlock_t ctl_files_rwlock;  /* ctl_files list lock */
    int controls_count;     /* count of all controls */
    int user_ctl_count;     /* count of all user controls */
    struct list_head controls;  /* all controls for this card */
    struct list_head ctl_files; /* active control files */
    struct mutex user_ctl_lock; /* protects user controls against concurrent access */

    struct snd_info_entry *proc_root;   /* root for soundcard specific files */
    struct snd_info_entry *proc_id; /* the card id */
    struct proc_dir_entry *proc_root_link;  /* number link to real id */

    struct list_head files_list;    /* all files associated to this card */
    struct snd_shutdown_f_ops *s_f_ops; /* file operations in the shutdown state */
    spinlock_t files_lock;      /* lock the files for this card */
    int shutdown;           /* this card is going down */
    struct completion *release_completion;
    struct device *dev;     /* device assigned to this card */
    struct device card_dev;     /* cardX object for sysfs */
    const struct attribute_group *dev_groups[4]; /* assigned sysfs attr */
    bool registered;        /* card_dev is registered? */  //用于标记是否已经注册

#ifdef CONFIG_PM
    unsigned int power_state;   /* power state */
    struct mutex power_lock;    /* power lock */
    wait_queue_head_t power_sleep;
#endif

#if defined(CONFIG_SND_MIXER_OSS) || defined(CONFIG_SND_MIXER_OSS_MODULE)
    struct snd_mixer_oss *mixer_oss;
    int mixer_oss_change_count;
#endif

};

// 将card_dev（snd_card的成员）转成snd_card
#define dev_to_snd_card(p)  container_of(p, struct snd_card, card_dev)



