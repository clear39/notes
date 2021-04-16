

// @ sound/core/device.c
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

    // 构建一个 snd_device   
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;
    
    //初始化 dev->list 
    INIT_LIST_HEAD(&dev->list);
    dev->card = card;
    dev->type = type;
    dev->state = SNDRV_DEV_BUILD;
    dev->device_data = device_data;  //控制设备为snd_card，其他的设备为 
    dev->ops = ops;

    /* insert the entry in an incrementally sorted list */
    list_for_each_prev(p, &card->devices) {
        struct snd_device *pdev = list_entry(p, struct snd_device, list);
        if ((unsigned int)pdev->type <= (unsigned int)type)
            break;
    }

    list_add(&dev->list, p);
    return 0;
}
EXPORT_SYMBOL(snd_device_new);

