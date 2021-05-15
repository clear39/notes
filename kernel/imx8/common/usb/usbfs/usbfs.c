
const struct file_operations usbdev_file_operations = {
    .owner =      THIS_MODULE,
    .llseek =     no_seek_end_llseek,
    .read =       usbdev_read,
    .poll =       usbdev_poll,
    .unlocked_ioctl = usbdev_ioctl,   // ioctl控制
#ifdef CONFIG_COMPAT
    .compat_ioctl =   usbdev_compat_ioctl,
#endif
    .mmap =       usbdev_mmap,
    .open =       usbdev_open,
    .release =    usbdev_release,
};

/*
 * file operations
 */
static int usbdev_open(struct inode *inode, struct file *file)
{
    struct usb_device *dev = NULL;
    struct usb_dev_state *ps;
    int ret;

    ret = -ENOMEM;
    ps = kzalloc(sizeof(struct usb_dev_state), GFP_KERNEL);
    if (!ps)
        goto out_free_ps;

    ret = -ENODEV;

    /* Protect against simultaneous removal or release */
    mutex_lock(&usbfs_mutex);

    /* usbdev device-node */
    if (imajor(inode) == USB_DEVICE_MAJOR)
        dev = usbdev_lookup_by_devt(inode->i_rdev);

    mutex_unlock(&usbfs_mutex);

    if (!dev)
        goto out_free_ps;

    usb_lock_device(dev);
    if (dev->state == USB_STATE_NOTATTACHED)
        goto out_unlock_device;

    ret = usb_autoresume_device(dev);
    if (ret)
        goto out_unlock_device;

    ps->dev = dev;
    ps->file = file;
    ps->interface_allowed_mask = 0xFFFFFFFF; /* 32 bits */
    spin_lock_init(&ps->lock);
    INIT_LIST_HEAD(&ps->list);
    INIT_LIST_HEAD(&ps->async_pending);
    INIT_LIST_HEAD(&ps->async_completed);
    INIT_LIST_HEAD(&ps->memory_list);
    init_waitqueue_head(&ps->wait);
    ps->disc_pid = get_pid(task_pid(current));
    ps->cred = get_current_cred();
    security_task_getsecid(current, &ps->secid);
    smp_wmb();
    list_add_tail(&ps->list, &dev->filelist);
    file->private_data = ps;
    usb_unlock_device(dev);
    snoop(&dev->dev, "opened by process %d: %s\n", task_pid_nr(current),current->comm);
    return ret;

 out_unlock_device:
    usb_unlock_device(dev);
    usb_put_dev(dev);
 out_free_ps:
    kfree(ps);
    return ret;
}


static long usbdev_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
    int ret;

    ret = usbdev_do_ioctl(file, cmd, (void __user *)arg);

    return ret;
}

// drivers/usb/core/devio.c
static bool usbfs_snoop;

#define snoop(dev, format, arg...)              \
    do {                            \
        if (usbfs_snoop)                \
            dev_info(dev, format, ## arg);      \
    } while (0)


/*
 * NOTE:  All requests here that have interface numbers as parameters
 * are assuming that somehow the configuration has been prevented from
 * changing.  But there's no mechanism to ensure that...
 */
static long usbdev_do_ioctl(struct file *file, unsigned int cmd,void __user *p)
{
    struct usb_dev_state *ps = file->private_data;
    struct inode *inode = file_inode(file);
    struct usb_device *dev = ps->dev;
    int ret = -ENOTTY;

    if (!(file->f_mode & FMODE_WRITE))
        return -EPERM;

    usb_lock_device(dev);

    /* Reap operations are allowed even after disconnection */
    switch (cmd) {
    case USBDEVFS_REAPURB:
        snoop(&dev->dev, "%s: REAPURB\n", __func__);
        ret = proc_reapurb(ps, p);
        goto done;

    case USBDEVFS_REAPURBNDELAY:
        snoop(&dev->dev, "%s: REAPURBNDELAY\n", __func__);
        ret = proc_reapurbnonblock(ps, p);
        goto done;

#ifdef CONFIG_COMPAT
    case USBDEVFS_REAPURB32:
        snoop(&dev->dev, "%s: REAPURB32\n", __func__);
        ret = proc_reapurb_compat(ps, p);
        goto done;

    case USBDEVFS_REAPURBNDELAY32:
        snoop(&dev->dev, "%s: REAPURBNDELAY32\n", __func__);
        ret = proc_reapurbnonblock_compat(ps, p);
        goto done;
#endif
    }                
    /**
     * 判断该usb设备节点的状态
    */
    if (!connected(ps)) {
        usb_unlock_device(dev);
        return -ENODEV;
    }

    switch (cmd) {
    case USBDEVFS_CONTROL://控制端点
        snoop(&dev->dev, "%s: CONTROL\n", __func__);
        ret = proc_control(ps, p);
        if (ret >= 0)
            inode->i_mtime = current_time(inode);
        break;

    case USBDEVFS_BULK://bulk传输
        snoop(&dev->dev, "%s: BULK\n", __func__);
        ret = proc_bulk(ps, p);
        if (ret >= 0)
            inode->i_mtime = current_time(inode);
        break;

    case USBDEVFS_RESETEP:
        snoop(&dev->dev, "%s: RESETEP\n", __func__);
        ret = proc_resetep(ps, p);
        if (ret >= 0)
            inode->i_mtime = current_time(inode);
        break;

    case USBDEVFS_RESET:
        snoop(&dev->dev, "%s: RESET\n", __func__);
        ret = proc_resetdevice(ps);
        break;
        
     case USBDEVFS_CLEAR_HALT:
        snoop(&dev->dev, "%s: CLEAR_HALT\n", __func__);
        ret = proc_clearhalt(ps, p);
        if (ret >= 0)
            inode->i_mtime = current_time(inode);
        break;

    case USBDEVFS_GETDRIVER:
        snoop(&dev->dev, "%s: GETDRIVER\n", __func__);
        ret = proc_getdriver(ps, p);
        break;

    case USBDEVFS_CONNECTINFO:
        snoop(&dev->dev, "%s: CONNECTINFO\n", __func__);
        ret = proc_connectinfo(ps, p);
        break;

    case USBDEVFS_SETINTERFACE:
        snoop(&dev->dev, "%s: SETINTERFACE\n", __func__);
        ret = proc_setintf(ps, p);
        break;

    case USBDEVFS_SETCONFIGURATION:
        snoop(&dev->dev, "%s: SETCONFIGURATION\n", __func__);
        ret = proc_setconfig(ps, p);
        break;

    case USBDEVFS_SUBMITURB:
        snoop(&dev->dev, "%s: SUBMITURB\n", __func__);
        ret = proc_submiturb(ps, p);
        if (ret >= 0)
            inode->i_mtime = current_time(inode);
        break;

#ifdef CONFIG_COMPAT
    case USBDEVFS_CONTROL32:
    snoop(&dev->dev, "%s: CONTROL32\n", __func__);
        ret = proc_control_compat(ps, p);
        if (ret >= 0)
            inode->i_mtime = current_time(inode);
        break;

    case USBDEVFS_BULK32:
        snoop(&dev->dev, "%s: BULK32\n", __func__);
        ret = proc_bulk_compat(ps, p);
        if (ret >= 0)
            inode->i_mtime = current_time(inode);
        break;

    case USBDEVFS_DISCSIGNAL32:
        snoop(&dev->dev, "%s: DISCSIGNAL32\n", __func__);
        ret = proc_disconnectsignal_compat(ps, p);
        break;

    case USBDEVFS_SUBMITURB32:
        snoop(&dev->dev, "%s: SUBMITURB32\n", __func__);
        ret = proc_submiturb_compat(ps, p);
        if (ret >= 0)
            inode->i_mtime = current_time(inode);
        break;

    case USBDEVFS_IOCTL32:
        snoop(&dev->dev, "%s: IOCTL32\n", __func__);
        ret = proc_ioctl_compat(ps, ptr_to_compat(p));
        break;
#endif

     case USBDEVFS_DISCARDURB:
        snoop(&dev->dev, "%s: DISCARDURB %pK\n", __func__, p);
        ret = proc_unlinkurb(ps, p);
        break;

    case USBDEVFS_DISCSIGNAL:
        snoop(&dev->dev, "%s: DISCSIGNAL\n", __func__);
        ret = proc_disconnectsignal(ps, p);
        break;
    
    case USBDEVFS_CLAIMINTERFACE:
        snoop(&dev->dev, "%s: CLAIMINTERFACE\n", __func__);
        /**
         * 驱动连接
        */
        ret = proc_claiminterface(ps, p);
        break;

    case USBDEVFS_RELEASEINTERFACE:
        snoop(&dev->dev, "%s: RELEASEINTERFACE\n", __func__);
        ret = proc_releaseinterface(ps, p);
        break;

    case USBDEVFS_IOCTL:
        snoop(&dev->dev, "%s: IOCTL\n", __func__);
        ret = proc_ioctl_default(ps, p);
        break;

    case USBDEVFS_CLAIM_PORT:
        snoop(&dev->dev, "%s: CLAIM_PORT\n", __func__);
        ret = proc_claim_port(ps, p);
        break;

    case USBDEVFS_RELEASE_PORT:
        snoop(&dev->dev, "%s: RELEASE_PORT\n", __func__);
        ret = proc_release_port(ps, p);
        break;
    case USBDEVFS_GET_CAPABILITIES:
        ret = proc_get_capabilities(ps, p);
        break;
    case USBDEVFS_DISCONNECT_CLAIM:
        ret = proc_disconnect_claim(ps, p);

    case USBDEVFS_ALLOC_STREAMS:
        ret = proc_alloc_streams(ps, p);
        break;
    case USBDEVFS_FREE_STREAMS:
        ret = proc_free_streams(ps, p);
        break;
    case USBDEVFS_DROP_PRIVILEGES:
        ret = proc_drop_privileges(ps, p);
        break;
    case USBDEVFS_GET_SPEED:
        ret = ps->dev->speed;
        break;
    }

 done:
    usb_unlock_device(dev);
    if (ret >= 0)
        inode->i_atime = current_time(inode);
    return ret;
}

static int connected(struct usb_dev_state *ps)                                                                                                                              
{
    return (!list_empty(&ps->list) &&
            ps->dev->state != USB_STATE_NOTATTACHED);
}




/**
 * 在 static int __init usb_init(void) 中调用
*/
int __init usb_devio_init(void)
{
    int retval;
    /**
     * 
    */
    retval = register_chrdev_region(USB_DEVICE_DEV, USB_DEVICE_MAX,"usb_device");
    if (retval) {
        printk(KERN_ERR "Unable to register minors for usb_device\n");
        goto out;
    }
    /**
     * static struct cdev usb_device_cdev;
     * 初始化 usb_device_cdev（cdev）,并将usbdev_file_operations赋值到 usb_device_cdev成员中
     * 
    */
    cdev_init(&usb_device_cdev, &usbdev_file_operations);
    /**
     * 将usb_device_cdev（cdev）加入到字符cdev_map（kobj_map有255个probe成员，用于存储字符设备链表）链表中
     * */                                                                                                             
    retval = cdev_add(&usb_device_cdev, USB_DEVICE_DEV, USB_DEVICE_MAX);
    if (retval) {
        printk(KERN_ERR "Unable to get usb_device major %d\n",USB_DEVICE_MAJOR);
        goto error_cdev;
    }
    /**
     *usbdev_nb @ drivers/usb/core/devio.c
     * 将usbdev_nb 加入到usb_notifier_list中，统一回调
    */
    usb_register_notify(&usbdev_nb);
out:
    return retval;

error_cdev:
    unregister_chrdev_region(USB_DEVICE_DEV, USB_DEVICE_MAX);
    goto out;
}





/////////////////////////////////////////////////////
static int proc_claiminterface(struct usb_dev_state *ps, void __user *arg)
{
	unsigned int ifnum;

	if (get_user(ifnum, (unsigned int __user *)arg))
		return -EFAULT;
	return claimintf(ps, ifnum);
}

static int claimintf(struct usb_dev_state *ps, unsigned int ifnum)
{
	struct usb_device *dev = ps->dev;
	struct usb_interface *intf;
	int err;

	if (ifnum >= 8*sizeof(ps->ifclaimed))
		return -EINVAL;
	/* already claimed */
	if (test_bit(ifnum, &ps->ifclaimed))
		return 0;

	if (ps->privileges_dropped && !test_bit(ifnum, &ps->interface_allowed_mask))
		return -EACCES;

	intf = usb_ifnum_to_if(dev, ifnum);
	if (!intf)
		err = -ENOENT;
	else
		err = usb_driver_claim_interface(&usbfs_driver, intf, ps);  // @ drivers/usb/core/driver.c
	if (err == 0)
		set_bit(ifnum, &ps->ifclaimed);
	return err;
}