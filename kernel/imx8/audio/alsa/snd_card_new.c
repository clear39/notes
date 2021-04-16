// @ sound/core/init.c

/**                                                                                                                                                                                                            
 *  snd_card_new - create and initialize a soundcard structure
 *  @parent: the parent device object
 *  @idx: card index (address) [0 ... (SNDRV_CARDS-1)]
 *  @xid: card identification (ASCII string)
 *  @module: top level module for locking
 *  @extra_size: allocate this extra size after the main soundcard structure
 *  @card_ret: the pointer to store the created card instance
 *
 *  Creates and initializes a soundcard structure.
 *
 *  The function allocates snd_card instance via kzalloc with the given
 *  space for the driver to use freely.  The allocated struct is stored
 *  in the given card_ret pointer.
 *
 *  Return: Zero if successful or a negative error code.
 */
int snd_card_new(struct device *parent, int idx, const char *xid,
            struct module *module, int extra_size,
            struct snd_card **card_ret)
{
    struct snd_card *card;
    int err;
    /**
     * 
    */
    if (snd_BUG_ON(!card_ret))
        return -EINVAL;
    *card_ret = NULL;

    /**
     * 
    */
    if (extra_size < 0)
        extra_size = 0;

    card = kzalloc(sizeof(*card) + extra_size, GFP_KERNEL);
    if (!card)
        return -ENOMEM;

    //指向snd_card最后
    if (extra_size > 0)
        card->private_data = (char *)card + sizeof(struct snd_card);

    // xid 存储到 card->id 中
    if (xid)
        strlcpy(card->id, xid, sizeof(card->id));

    err = 0;
    mutex_lock(&snd_card_mutex);
    if (idx < 0) /* first check the matching module-name slot */
        idx = get_slot_from_bitmask(idx, module_slot_match, module);

    if (idx < 0) /* if not matched, assign an empty slot */
        idx = get_slot_from_bitmask(idx, check_empty_slot, module);

    if (idx < 0)
        err = -ENODEV;
    else if (idx < snd_ecards_limit) {
        if (test_bit(idx, snd_cards_lock))
            err = -EBUSY;   /* invalid */
    } else if (idx >= SNDRV_CARDS)
        err = -ENODEV;


    if (err < 0) {
        mutex_unlock(&snd_card_mutex);
        dev_err(parent, "cannot find the slot for index %d (range 0-%i), error: %d\n",
             idx, snd_ecards_limit - 1, err);
        kfree(card);
        return err;
    }


    set_bit(idx, snd_cards_lock);       /* lock it */
    if (idx >= snd_ecards_limit)
        snd_ecards_limit = idx + 1; /* increase the limit */
    mutex_unlock(&snd_card_mutex);
    card->dev = parent;
    // card->number 保存的是第几张声卡
    card->number = idx;
    // 
    card->module = module;
    // 初始化 card->devices 列表
    INIT_LIST_HEAD(&card->devices);
    // 
    init_rwsem(&card->controls_rwsem);
    rwlock_init(&card->ctl_files_rwlock);
    mutex_init(&card->user_ctl_lock);
    // 初始化 card->controls 列表
    INIT_LIST_HEAD(&card->controls);
    spin_lock_init(&card->files_lock);
    // 初始化 card->files_list 列表
    INIT_LIST_HEAD(&card->files_list);

#ifdef CONFIG_PM
    mutex_init(&card->power_lock);
    init_waitqueue_head(&card->power_sleep);
#endif

    // device 内核通用初始化函数, device_initialize @   drivers/base/core.c
    device_initialize(&card->card_dev);
    card->card_dev.parent = parent;
    card->card_dev.class = sound_class;
    card->card_dev.release = release_card_device;
    card->card_dev.groups = card->dev_groups;
    card->dev_groups[0] = &card_dev_attr_group;

    err = kobject_set_name(&card->card_dev.kobj, "card%d", idx);
    if (err < 0)
        goto __error;

    /* Interrupt description */
    snprintf(card->irq_descr, sizeof(card->irq_descr), "%s:%s", dev_driver_string(card->dev), dev_name(&card->card_dev));

    /* the control interface cannot be accessed from the user space until */
    /* snd_cards_bitmask and snd_cards are set with snd_card_register */
     /**
     * 创建控制设备snd_device（SNDRV_DEV_CONTROL），并且将snd_device成员list 添加到 card->devices 链表上
    */
    err = snd_ctl_create(card);
    if (err < 0) {
        dev_err(parent, "unable to register control minors\n");
        goto __error;
    }
    // 在/proc/asound字幕下创建对应声卡cardx目录
    err = snd_info_card_create(card);
    if (err < 0) {
        dev_err(parent, "unable to create card info\n");
        goto __error_ctl;
    }
    *card_ret = card;
    return 0;

__error_ctl:
    snd_device_free_all(card);
__error:
    put_device(&card->card_dev);
    return err;
}
EXPORT_SYMBOL(snd_card_new);      


/*
 * create control core:
 * called from init.c
 */ 
int snd_ctl_create(struct snd_card *card)                                                                                                                                                                      
{   
    static struct snd_device_ops ops = {
        .dev_free = snd_ctl_dev_free,
        .dev_register = snd_ctl_dev_register,   // __snd_device_register 中调用
        .dev_disconnect = snd_ctl_dev_disconnect,
    };  
    int err;
    
    if (snd_BUG_ON(!card))
        return -ENXIO;

    if (snd_BUG_ON(card->number < 0 || card->number >= SNDRV_CARDS))
        return -ENXIO;

    /* card->ctl_dev 为 control device */
    snd_device_initialize(&card->ctl_dev, card);
    // 设置card->ctl_dev的成员name
    dev_set_name(&card->ctl_dev, "controlC%d", card->number);
    /**
     * 创建控制设备snd_device，并且将snd_device成员list 添加到 card->devices 链表上
    */
    err = snd_device_new(card, SNDRV_DEV_CONTROL, card, &ops);
    
    if (err < 0)
        put_device(&card->ctl_dev);

    return err;
}

/**
 * snd_device_initialize - Initialize struct device for sound devices
 * @dev: device to initialize
 * @card: card to assign, optional                                                                                                                                                                             
 */
void snd_device_initialize(struct device *dev, struct snd_card *card)
{
    //通用初始化 dev 
    device_initialize(dev);
    if (card)
        dev->parent = &card->card_dev;

    dev->class = sound_class;
    dev->release = default_release;
}
EXPORT_SYMBOL_GPL(snd_device_initialize);


/**
 * snd_device_new - create an ALSA device component
 * @card: the card instance
 * @type: the device type, SNDRV_DEV_XXX
 * @device_data: the data pointer of this device
 * @ops: the operator table
 *
 * Creates a new device component for the given data pointer.
 * The device will be assigned to the card and managed together
 * by the card.
 *
 * The data pointer plays a role as the identifier, too, so the
 * pointer address must be unique and unchanged.
 *
 * Return: Zero if successful, or a negative error code on failure.
 */
int snd_device_new(struct snd_card *card, enum snd_device_type type, void *device_data, struct snd_device_ops *ops)
{
    struct snd_device *dev;
    struct list_head *p; 

    if (snd_BUG_ON(!card || !device_data || !ops))
        return -ENXIO;

    // 分配 snd_device 空间
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;
    
    //初始化 snd_device中成员 list（list of registered devices）
    INIT_LIST_HEAD(&dev->list);
    dev->card = card;
    dev->type = type;
    dev->state = SNDRV_DEV_BUILD;
    dev->device_data = device_data;
    dev->ops = ops;

    /**
     * card->devices 中存储的是 snd_device成员list
     * 
    */
    /* insert the entry in an incrementally sorted list */
    list_for_each_prev(p, &card->devices) { // card->devices 为 list_head
        // p 为 snd_device成员list，转化成 snd_device
        // 查找对对应类型的链表位置p，说明card->devices是按 type 进行存储
        struct snd_device *pdev = list_entry(p, struct snd_device, list);
        if ((unsigned int)pdev->type <= (unsigned int)type)
            break;
    }   

    // 将 dev->list 添加到 p 链表上 
    list_add(&dev->list, p); 
    return 0;
}
EXPORT_SYMBOL(snd_device_new);

///////////////////////////////////////////////////////////////
/*
 * create a card proc file
 * called from init.c
 */ 
int snd_info_card_create(struct snd_card *card)                                                                                                                                                                
{   
    char str[8];
    struct snd_info_entry *entry;
    
    if (snd_BUG_ON(!card))
        return -ENXIO;
        
    sprintf(str, "card%i", card->number);
    entry = create_subdir(card->module, str);
    if (!entry) 
        return -ENOMEM;
    card->proc_root = entry;
    return 0;
}

//  @   sound/core/info.c
static struct snd_info_entry *create_subdir(struct module *mod, const char *name)
{
    struct snd_info_entry *entry;

    entry = snd_info_create_module_entry(mod, name, NULL);
    if (!entry)
        return NULL;
    entry->mode = S_IFDIR | S_IRUGO | S_IXUGO;
    if (snd_info_register(entry) < 0) {
        snd_info_free_entry(entry);
        return NULL;
    }   
    return entry;
}

/**
 * snd_info_create_module_entry - create an info entry for the given module
 * @module: the module pointer
 * @name: the file name
 * @parent: the parent directory
 *
 * Creates a new info entry and assigns it to the given module.
 *
 * Return: The pointer of the new instance, or %NULL on failure.
 */
struct snd_info_entry *snd_info_create_module_entry(struct module * module,                                                                                                                                    
                           const char *name,
                           struct snd_info_entry *parent)
{
    struct snd_info_entry *entry = snd_info_create_entry(name, parent);
    if (entry)
        entry->module = module;
    return entry;
}
EXPORT_SYMBOL(snd_info_create_module_entry);


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



/** 
 * snd_info_register - register the info entry
 * @entry: the info entry
 *
 * Registers the proc info entry.
 *
 * Return: Zero if successful, or a negative error code on failure.
 */
int snd_info_register(struct snd_info_entry * entry)
{
    struct proc_dir_entry *root, *p = NULL;

    if (snd_BUG_ON(!entry))
        return -ENXIO;
    
    // 
    root = entry->parent == NULL ? snd_proc_root->p : entry->parent->p;
    mutex_lock(&info_mutex);
    if (S_ISDIR(entry->mode)) {
        p = proc_mkdir_mode(entry->name, entry->mode, root);
        if (!p) {
            mutex_unlock(&info_mutex);
            return -ENOMEM;
        }
    } else {
        const struct file_operations *ops;
        // 
        if (entry->content == SNDRV_INFO_CONTENT_DATA)
            ops = &snd_info_entry_operations;
        else
            ops = &snd_info_text_entry_ops;
        // 
        p = proc_create_data(entry->name, entry->mode, root,ops, entry);
        if (!p) {
            mutex_unlock(&info_mutex);
            return -ENOMEM;
        }
        proc_set_size(p, entry->size);
    }
    entry->p = p;
    mutex_unlock(&info_mutex);
    return 0;
}

EXPORT_SYMBOL(snd_info_register);




