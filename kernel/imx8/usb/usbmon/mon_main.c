
//整个usbmon的入口
module_init(mon_init);

static int __init mon_init(void)
{
	struct usb_bus *ubus;
	int rc, id;

	/**
	* 在 debugfs 目录下创建 usbmon 目录
	*/
    //  @   drivers/usb/mon/mon_text.c
	if ((rc = mon_text_init()) != 0)
		goto err_text;

    //  @   drivers/usb/mon/mon_bin.c
    //
	if ((rc = mon_bin_init()) != 0)
		goto err_bin;

    //  
	mon_bus0_init();

	if (usb_mon_register(&mon_ops_0) != 0) {
		printk(KERN_NOTICE TAG ": unable to register with the core\n");
		rc = -ENODEV;
		goto err_reg;
	}
	// MOD_INC_USE_COUNT(which_module?);

	mutex_lock(&usb_bus_idr_lock);

	idr_for_each_entry(&usb_bus_idr, ubus, id)
		mon_bus_init(ubus);

	usb_register_notify(&mon_nb);
	mutex_unlock(&usb_bus_idr_lock);
	return 0;

err_reg:
	mon_bin_exit();
err_bin:
	mon_text_exit();
err_text:
	return rc;
}

//  @   drivers/usb/mon/mon_text.c
int __init mon_text_init(void)
{
	struct dentry *mondir;

	mondir = debugfs_create_dir("usbmon", usb_debug_root);
	if (IS_ERR(mondir)) {
		/* debugfs not available, but we can use usbmon without it */
		return 0;
	}
	if (mondir == NULL) {
		printk(KERN_NOTICE TAG ": unable to create usbmon directory\n");
		return -ENOMEM;
	}
	mon_dir = mondir;
	return 0;
}

//  @   drivers/usb/mon/mon_bin.c
int __init mon_bin_init(void)
{
	int rc;
    // 在 /sys/classes/usbmon
	mon_bin_class = class_create(THIS_MODULE, "usbmon");
	if (IS_ERR(mon_bin_class)) {
		rc = PTR_ERR(mon_bin_class);
		goto err_class;
	}

    //  drivers/usb/mon/mon_bin.c:170:#define MON_BIN_MAX_MINOR 128
    //  @   fs/char_dev.c
	//	static dev_t mon_bin_dev0;
	//	alloc_chrdev_region 中创建 char_device_struct
	rc = alloc_chrdev_region(&mon_bin_dev0, 0, MON_BIN_MAX_MINOR, "usbmon");
	if (rc < 0)
		goto err_dev;

	//	static struct cdev mon_bin_cdev;
	cdev_init(&mon_bin_cdev, &mon_fops_binary);
	mon_bin_cdev.owner = THIS_MODULE;

    //  drivers/usb/mon/mon_bin.c:170:#define MON_BIN_MAX_MINOR 128
	rc = cdev_add(&mon_bin_cdev, mon_bin_dev0, MON_BIN_MAX_MINOR);
	if (rc < 0)
		goto err_add;

	return 0;

err_add:
	unregister_chrdev_region(mon_bin_dev0, MON_BIN_MAX_MINOR);
err_dev:
	class_destroy(mon_bin_class);
err_class:
	return rc;
}



static void mon_bus0_init(void)                                                                                                                                                                                
{
    struct mon_bus *mbus = &mon_bus0;

    kref_init(&mbus->ref);
    spin_lock_init(&mbus->lock);
    INIT_LIST_HEAD(&mbus->r_list);

    // mon_text_add 在debugfs/usbmon/
    mbus->text_inited = mon_text_add(mbus, NULL);

    //
    mbus->bin_inited = mon_bin_add(mbus, NULL);
}


int mon_text_add(struct mon_bus *mbus, const struct usb_bus *ubus)
{
    struct dentry *d;
    enum { NAMESZ = 10 };
    char name[NAMESZ];
    int busnum = ubus? ubus->busnum: 0;
    int rc;

    if (mon_dir == NULL)
        return 0;

    if (ubus != NULL) {
        rc = snprintf(name, NAMESZ, "%dt", busnum);
        if (rc <= 0 || rc >= NAMESZ)
            goto err_print_t;
        d = debugfs_create_file(name, 0600, mon_dir, mbus,
                                 &mon_fops_text_t);
        if (d == NULL)
            goto err_create_t;
        mbus->dent_t = d;
    }

    rc = snprintf(name, NAMESZ, "%du", busnum);
    if (rc <= 0 || rc >= NAMESZ)
        goto err_print_u;
    d = debugfs_create_file(name, 0600, mon_dir, mbus, &mon_fops_text_u);
    if (d == NULL)
        goto err_create_u;
    mbus->dent_u = d;

    rc = snprintf(name, NAMESZ, "%ds", busnum);
    if (rc <= 0 || rc >= NAMESZ)
        goto err_print_s;
    d = debugfs_create_file(name, 0600, mon_dir, mbus, &mon_fops_stat);
    if (d == NULL)
        goto err_create_s;
    mbus->dent_s = d;

    return 1;

err_create_s:
err_print_s:
    debugfs_remove(mbus->dent_u);
    mbus->dent_u = NULL;
err_create_u:
err_print_u:
    if (ubus != NULL) {
        debugfs_remove(mbus->dent_t);
        mbus->dent_t = NULL;
    }
err_create_t:
err_print_t:
    return 0;
}

int mon_bin_add(struct mon_bus *mbus, const struct usb_bus *ubus)                                                                                                                                              
{
    struct device *dev;
    unsigned minor = ubus? ubus->busnum: 0;

    if (minor >= MON_BIN_MAX_MINOR)
        return 0;

    dev = device_create(mon_bin_class, ubus ? ubus->controller : NULL,
                MKDEV(MAJOR(mon_bin_dev0), minor), NULL, "usbmon%d", minor);
    if (IS_ERR(dev))
        return 0;

    mbus->classdev = dev;
    return 1;
}   
   
/*
 * The registration is unlocked.
 * We do it this way because we do not want to lock in hot paths.
 *
 * Notice that the code is minimally error-proof. Because usbmon needs
 * symbols from usbcore, usbcore gets referenced and cannot be unloaded first.
 */

int usb_mon_register(const struct usb_mon_operations *ops)
{

	if (mon_ops)
		return -EBUSY;

	mon_ops = ops;
	mb();
	return 0;
}
EXPORT_SYMBOL_GPL (usb_mon_register);
