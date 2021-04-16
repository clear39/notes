//这里以UVC驱动为例
// @ drivers/media/usb/uvc/uvc_driver.c
static int __init uvc_init(void)
{
	int ret;

	uvc_printk("xqli %s",__func__);

	/**
	 * 
	*/
	uvc_debugfs_init();
	//任何usb上层驱动，都是以usb_register进行注册，需要重点进行分析
	ret = usb_register(&uvc_driver.driver);
	if (ret < 0) {
		uvc_debugfs_cleanup();
		return ret;
	}

	printk(KERN_INFO DRIVER_DESC " (" DRIVER_VERSION ")\n");
	return 0;
}

//uvc_driver 定义 
// @ drivers/media/usb/uvc/uvcvideo.h
struct uvc_driver {
	struct usb_driver driver;
};
	
// @ drivers/media/usb/uvc/uvcvideo.h
// uvc_driver初始化
struct uvc_driver uvc_driver = {
	.driver = {
		.name		= "uvcvideo",
		.probe		= uvc_probe,
		.disconnect	= uvc_disconnect,
		.suspend	= uvc_suspend,
		.resume		= uvc_resume,
		.reset_resume	= uvc_reset_resume,
		.id_table	= uvc_ids,
		.supports_autosuspend = 1,
	},
};




// @ include/linux/usb.h
/* use a define to avoid include chaining to get THIS_MODULE & friends */
#define usb_register(driver) \
	usb_register_driver(driver, THIS_MODULE, KBUILD_MODNAME)


// @ drivers/usb/core/driver.c
/**
 * usb_register_driver - register a USB interface driver
 * @new_driver: USB operations for the interface driver
 * @owner: module owner of this driver.
 * @mod_name: module name string
 *
 * Registers a USB interface driver with the USB core.  The list of
 * unattached interfaces will be rescanned whenever a new driver is
 * added, allowing the new driver to attach to any recognized interfaces.
 *
 * Return: A negative error code on failure and 0 on success.
 *
 * NOTE: if you want your driver to use the USB major number, you must call
 * usb_register_dev() to enable that functionality.  This function no longer
 * takes care of that.
 */
int usb_register_driver(struct usb_driver *new_driver, struct module *owner,
			const char *mod_name)
{
	int retval = 0;
	
	//
	if (usb_disabled())
		return -ENODEV;

	new_driver->drvwrap.for_devices = 0;
	new_driver->drvwrap.driver.name = new_driver->name;   //驱动名称
	new_driver->drvwrap.driver.bus = &usb_bus_type;	     //总线类型
	new_driver->drvwrap.driver.probe = usb_probe_interface;   //注意这里的probe函数为usb接口probe函数
	new_driver->drvwrap.driver.remove = usb_unbind_interface;
	new_driver->drvwrap.driver.owner = owner;
	new_driver->drvwrap.driver.mod_name = mod_name;
	spin_lock_init(&new_driver->dynids.lock);
	INIT_LIST_HEAD(&new_driver->dynids.list);
	
	//再次将通用driver注册到系统中
	retval = driver_register(&new_driver->drvwrap.driver);
	if (retval)
		goto out;

	retval = usb_create_newid_files(new_driver);
	if (retval)
		goto out_newid;

	pr_info("%s: registered new interface driver %s\n",
			usbcore_name, new_driver->name);
usb_driver
out:
	return retval;

out_newid:
	driver_unregister(&new_driver->drvwrap.driver);

	printk(KERN_ERR "%s: error %d registering interface "
			"	driver %s\n",
			usbcore_name, retval, new_driver->name);
	goto out;
}
EXPORT_SYMBOL_GPL(usb_register_driver);


















