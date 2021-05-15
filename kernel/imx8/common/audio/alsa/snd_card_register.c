
//  @   sound/core/init.c
/**
 *  snd_card_register - register the soundcard
 *  @card: soundcard structure
 *
 *  This function registers all the devices assigned to the soundcard.
 *  Until calling this, the ALSA control interface is blocked from the
 *  external accesses.  Thus, you should call this function at the end
 *  of the initialization of the card.
 *
 *  Return: Zero otherwise a negative error code if the registration failed.
 * 
 * 
 * 
 */
int snd_card_register(struct snd_card *card)
{
    int err; 

    if (snd_BUG_ON(!card))
        return -EINVAL;

    if (!card->registered) {
        // 通用device函数调用，@ drivers/base/core.c
        err = device_add(&card->card_dev);
        if (err < 0) 
            return err; 
        card->registered = true;
    }    

    // 
    if ((err = snd_device_register_all(card)) < 0) 
        return err; 


    mutex_lock(&snd_card_mutex);
    if (snd_cards[card->number]) {
        /* already registered */
        mutex_unlock(&snd_card_mutex);
        return snd_info_card_register(card); /* register pending info */
    }    
    if (*card->id) {
        /* make a unique id name from the given string */
        char tmpid[sizeof(card->id)];
        memcpy(tmpid, card->id, sizeof(card->id));
        snd_card_set_id_no_lock(card, tmpid, tmpid);
    } else {
        /* create an id from either shortname or longname */
        const char *src;
        src = *card->shortname ? card->shortname : card->longname;
        snd_card_set_id_no_lock(card, src, 
                    retrieve_id_from_card_name(src));
    }    
    snd_cards[card->number] = card;
    mutex_unlock(&snd_card_mutex);
    init_info_for_card(card);
#if IS_ENABLED(CONFIG_SND_MIXER_OSS)
    if (snd_mixer_oss_notify_callback)
        snd_mixer_oss_notify_callback(card, SND_MIXER_OSS_NOTIFY_REGISTER);
#endif
    return 0;
}

EXPORT_SYMBOL(snd_card_register);




//  @   sound/core/device.c
/*
 * register all the devices on the card.
 * called from init.c
 */
int snd_device_register_all(struct snd_card *card)
{
    struct snd_device *dev;
    int err;
        
    if (snd_BUG_ON(!card))
        return -ENXIO;
    
    /**
     * dev 为 snd_device(list为他第一个成员)
    */
    list_for_each_entry(dev, &card->devices, list) {                                                                                                                                                           
        err = __snd_device_register(dev);
        if (err < 0)
            return err;
    }   
    return 0;
}


static int __snd_device_register(struct snd_device *dev)                                                                                                                                                       
{   
    // SNDRV_DEV_BUILD 在 snd_device_new 中赋值
    if (dev->state == SNDRV_DEV_BUILD) {
        // 
        if (dev->ops->dev_register) {
            //这里我们以 controlC%d 设备的注册函数，详情请看 snd_card_new-> snd_ctl_create 函数
            int err = dev->ops->dev_register(dev);
            if (err < 0)
                return err;
        }
        dev->state = SNDRV_DEV_REGISTERED;  //状态更新为已经注册
    }
    return 0;
}

//  @   sound/core/control.c
static const struct file_operations snd_ctl_f_ops =                                                                                                                                                            
{
    .owner =    THIS_MODULE,
    .read =     snd_ctl_read,
    .open =     snd_ctl_open,
    .release =  snd_ctl_release,
    .llseek =   no_llseek,
    .poll =     snd_ctl_poll,
    .unlocked_ioctl =   snd_ctl_ioctl,
    .compat_ioctl = snd_ctl_ioctl_compat,
    .fasync =   snd_ctl_fasync,
};




//这里我们以 controlC%d 设备的注册函数，详情请看 snd_card_new-> snd_ctl_create 函数
/**
 * registration of the control device
 * 
 * 调用过程如下：
 * int snd_card_register(struct snd_card *card)
 * --> int snd_device_register_all(struct snd_card *card)
 * ----> static int __snd_device_register(struct snd_device *dev)         
 */
static int snd_ctl_dev_register(struct snd_device *device)                                                                                                                                                     
{
    struct snd_card *card = device->device_data;

    return snd_register_device(SNDRV_DEVICE_TYPE_CONTROL, card, -1,&snd_ctl_f_ops, card, &card->ctl_dev);
}


//  @   sound/core/sound.c
/**
 * snd_register_device - Register the ALSA device file for the card
 * @type: the device type, SNDRV_DEVICE_TYPE_XXX
 * @card: the card instance
 * @dev: the device index
 * @f_ops: the file operations
 * @private_data: user pointer for f_ops->open()
 * @device: the device to register
 *
 * Registers an ALSA device file for the given card.
 * The operators have to be set in reg parameter.
 *
 * Return: Zero if successful, or a negative error code on failure.
 */
int snd_register_device(int type, struct snd_card *card, int dev,
            const struct file_operations *f_ops,
            void *private_data, struct device *device)
{
    int minor;
    int err = 0;
    struct snd_minor *preg;

    if (snd_BUG_ON(!device))
        return -EINVAL;
    /*
    struct snd_minor {                                                                                                                                                                                             
        int type;           //  SNDRV_DEVICE_TYPE_XXX 
        int card;           // card number 
        int device;         // device number 
        const struct file_operations *f_ops;    // file operations
        void *private_data;     // private data for f_ops->open
        struct device *dev;     // device for sysfs 
        struct snd_card *card_ptr;  // assigned card instance
    }; 
    */
    preg = kmalloc(sizeof *preg, GFP_KERNEL);
    if (preg == NULL)
        return -ENOMEM;

    preg->type = type;
    preg->card = card ? card->number : -1;
    preg->device = dev;
    preg->f_ops = f_ops;
    preg->private_data = private_data;
    preg->card_ptr = card;
    mutex_lock(&sound_mutex);
    // CONFIG_SND_DYNAMIC_MINORS一般没有定义，
    // 计算得到此设备id
    minor = snd_find_free_minor(type, card, dev);
    if (minor < 0) {
        err = minor;
        goto error;
    }

    preg->dev = device;
    //  static int major = CONFIG_SND_MAJOR;  @ sound/core/sound.c
    device->devt = MKDEV(major, minor);

    //通用设备添加
    err = device_add(device);
    if (err < 0)
        goto error;

    //  #define SNDRV_OS_MINORS         256
    //  static struct snd_minor *snd_minors[SNDRV_OS_MINORS];  
    snd_minors[minor] = preg;
 error:
    mutex_unlock(&sound_mutex);
    if (err < 0)
        kfree(preg);
    return err;
}
EXPORT_SYMBOL(snd_register_device);

static int snd_find_free_minor(int type, struct snd_card *card, int dev)
{
    int minor;

    switch (type) {
    case SNDRV_DEVICE_TYPE_SEQUENCER:
    case SNDRV_DEVICE_TYPE_TIMER:
        minor = type;
        break;
    case SNDRV_DEVICE_TYPE_CONTROL:
        if (snd_BUG_ON(!card))
            return -EINVAL;
        minor = SNDRV_MINOR(card->number, type);
        break;
    case SNDRV_DEVICE_TYPE_HWDEP:
    case SNDRV_DEVICE_TYPE_RAWMIDI:
    case SNDRV_DEVICE_TYPE_PCM_PLAYBACK:
    case SNDRV_DEVICE_TYPE_PCM_CAPTURE:
    case SNDRV_DEVICE_TYPE_COMPRESS:
        if (snd_BUG_ON(!card))
            return -EINVAL;
        minor = SNDRV_MINOR(card->number, type + dev);
        break;
    default:
        return -EINVAL;
    }
    if (snd_BUG_ON(minor < 0 || minor >= SNDRV_OS_MINORS))
        return -EINVAL;
    if (snd_minors[minor])
        return -EBUSY;
    return minor;
}
