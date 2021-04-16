
//  @   sound/core/sound.c

subsys_initcall(alsa_sound_init);
module_exit(alsa_sound_exit);

/*
 *  INIT PART
 */
static int __init alsa_sound_init(void)
{
    // static int major = CONFIG_SND_MAJOR;
    snd_major = major;
    //  static int cards_limit = 1;
    snd_ecards_limit = cards_limit;
    /*
    static const struct file_operations snd_fops =
    {
        .owner =    THIS_MODULE,
        .open =     snd_open,
        .llseek =   noop_llseek,
    }; 
    */
    if (register_chrdev(major, "alsa", &snd_fops)) {
        pr_err("ALSA core: unable to register native major device number %d\n", major);
        return -EIO;
    }   
    if (snd_info_init() < 0) {                                                                                                                                                                                 
        unregister_chrdev(major, "alsa");
        return -ENOMEM;
    }   
#ifndef MODULE
    pr_info("Advanced Linux Sound Architecture Driver Initialized.\n");
#endif
    return 0;
}

static int snd_open(struct inode *inode, struct file *file)                                                                                                                                                    
{
    unsigned int minor = iminor(inode);
    struct snd_minor *mptr = NULL;
    const struct file_operations *new_fops;
    int err = 0;

    if (minor >= ARRAY_SIZE(snd_minors))
        return -ENODEV;

    mutex_lock(&sound_mutex);
    mptr = snd_minors[minor];
    if (mptr == NULL) {
        mptr = autoload_device(minor);
        if (!mptr) {
            mutex_unlock(&sound_mutex);
            return -ENODEV;
        }
    }
    new_fops = fops_get(mptr->f_ops);
    mutex_unlock(&sound_mutex);
    if (!new_fops)
        return -ENODEV;
    replace_fops(file, new_fops);

    if (file->f_op->open)
        err = file->f_op->open(inode, file);
    return err;
}


//  @   sound/core/info.c
int __init snd_info_init(void)
{
    // 构建一个snd_info_entry
    snd_proc_root = snd_info_create_entry("asound", NULL);
    if (!snd_proc_root)
        return -ENOMEM;
    snd_proc_root->mode = S_IFDIR | S_IRUGO | S_IXUGO;
    // 
    snd_proc_root->p = proc_mkdir("asound", NULL);
    if (!snd_proc_root->p)
        goto error;
#ifdef CONFIG_SND_OSSEMUL
    snd_oss_root = create_subdir(THIS_MODULE, "oss");
    if (!snd_oss_root)
        goto error;
#endif
#if IS_ENABLED(CONFIG_SND_SEQUENCER)
    snd_seq_root = create_subdir(THIS_MODULE, "seq");
    if (!snd_seq_root)
        goto error;
#endif
    if (snd_info_version_init() < 0 ||
        snd_minor_info_init() < 0 ||
        snd_minor_info_oss_init() < 0 ||
        snd_card_info_init() < 0 ||
        snd_info_minor_register() < 0)
        goto error;
    return 0;

 error:
    snd_info_free_entry(snd_proc_root);
    return -ENOMEM;
}


/*
 * snd_info_create_entry - create an info entry
 * @name: the proc file name
 * @parent: the parent directory
 *
 * Creates an info entry with the given file name and initializes as
 * the default state.
 *
 * Usually called from other functions such as
 * snd_info_create_card_entry().
 *
 * Return: The pointer of the new instance, or %NULL on failure.
 */
static struct snd_info_entry *
snd_info_create_entry(const char *name, struct snd_info_entry *parent)
{
    struct snd_info_entry *entry;
    entry = kzalloc(sizeof(*entry), GFP_KERNEL);                                                                                                                                                               
    if (entry == NULL)
        return NULL;
    entry->name = kstrdup(name, GFP_KERNEL);
    if (entry->name == NULL) {
        kfree(entry);
        return NULL;
    }
    entry->mode = S_IFREG | S_IRUGO;
    entry->content = SNDRV_INFO_CONTENT_TEXT;
    mutex_init(&entry->access);
    INIT_LIST_HEAD(&entry->children);
    INIT_LIST_HEAD(&entry->list);
    entry->parent = parent;
    if (parent)
        list_add_tail(&entry->list, &parent->children);
    return entry;
}



