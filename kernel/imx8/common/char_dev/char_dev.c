//  @   fs/char_dev.c

/**
 * register_chrdev_region() - register a range of device numbers
 * @from: the first in the desired range of device numbers; must include
 *        the major number.
 * @count: the number of consecutive device numbers required
 * @name: the name of the device or driver.
 *
 * Return value is zero on success, a negative error code on failure.
 */
int register_chrdev_region(dev_t from, unsigned count, const char *name)                                                                                                                                       
{
    struct char_device_struct *cd;
    dev_t to = from + count;  //从usb的调用来看，count=64*128;from=(189<<8) | 0
    dev_t n, next;

    for (n = from; n < to; n = next) {
        /**
         * 
        */
        next = MKDEV(MAJOR(n)+1, 0);
        if (next > to)
            next = to;
        /**
         * 
        */
        cd = __register_chrdev_region(MAJOR(n), MINOR(n),next - n, name);
        if (IS_ERR(cd))
            goto fail;
    }
    return 0;

fail:
    to = n;
    for (n = from; n < to; n = next) {
        next = MKDEV(MAJOR(n)+1, 0);
        kfree(__unregister_chrdev_region(MAJOR(n), MINOR(n), next - n));
    }
    return PTR_ERR(cd);
}


/**
 * alloc_chrdev_region() - register a range of char device numbers
 * @dev: output parameter for first assigned number
 * @baseminor: first of the requested range of minor numbers  //
 * @count: the number of minor numbers required
 * @name: the name of the associated device or driver
 *
 * Allocates a range of char device numbers.  The major number will be
 * chosen dynamically, and returned (along with the first minor number)
 * in @dev.  Returns zero or a negative error code.
 */
int alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count, const char *name)
{
    struct char_device_struct *cd;
    /**
     * 这里 __register_chrdev_region 第一个参数为0，表示动态分配主id
    */
    cd = __register_chrdev_region(0, baseminor, count, name);
    if (IS_ERR(cd))
        return PTR_ERR(cd);
    /**
     * MKDEV 将cd->major(高8位)和cd->baseminor(低8位)转成2字节
    */
    *dev = MKDEV(cd->major, cd->baseminor);
    return 0;
}

/*
 * Register a single major with a specified minor range.
 *
 * If major == 0 this functions will dynamically allocate a major and return
 * its number. //动态分配主id
 *
 * If major > 0 this function will attempt to reserve the passed range of
 * minors and will return zero on success.
 *
 * Returns a -ve errno on failure.
 */
 static struct char_device_struct *
__register_chrdev_region(unsigned int major, unsigned int baseminor, int minorct, const char *name)
{
    struct char_device_struct *cd, **cp;
    int ret = 0;
    int i;

    /**
     * 构建char_device_struct
    */
    cd = kzalloc(sizeof(struct char_device_struct), GFP_KERNEL);
    if (cd == NULL)
        return ERR_PTR(-ENOMEM);

    mutex_lock(&chrdevs_lock);

    //如果主设备id为0，则通过find_dynamic_major查找空闲的主设备id；
    if (major == 0) {
        /**
         * 
        */
        ret = find_dynamic_major();
        if (ret < 0) {
            pr_err("CHRDEV \"%s\" dynamic allocation region is full\n", name);
            goto out;
        }
        major = ret;
    }

    /***
     *   include/linux/fs.h:2526:
     * #define CHRDEV_MAJOR_MAX 512
     * 主设备id不能大于等于512
     * */
    if (major >= CHRDEV_MAJOR_MAX) {
        pr_err("CHRDEV \"%s\" major requested (%d) is greater than the maximum (%d)\n", name, major, CHRDEV_MAJOR_MAX);
        ret = -EINVAL;
        goto out;
    }

    cd->major = major;  //主设备id
    cd->baseminor = baseminor; //从设备id的起始值
    cd->minorct = minorct;//从设备的个数
    strlcpy(cd->name, name, sizeof(cd->name));//该种设备的驱动名称，设备名称
    
    /**
     * major_to_index内部执行 major % CHRDEV_MAJOR_HASH_SIZE(255)，去余数值
     * 由于major只能小于512，则major_to_index得到的值有0～254
    */
    i = major_to_index(major);

    /***
     *  chrdevs为char_device_struct 
     *  *chrdevs[CHRDEV_MAJOR_HASH_SIZE]  // CHRDEV_MAJOR_HASH_SIZE =255
     *  chrdevs为一个数组指针，共255个成员，每一个都为一个指针
     * 
     * 注意for循环中 *cp 判断非空
     * 
     * 
     * 在chrdevs数组中找对应的major成员，再对该成员链表进行遍历，找到对应的位置
     * */
    for (cp = &chrdevs[i]; *cp; cp = &(*cp)->next)
        /**
         * 找一个主id仅大于major成员；
         * 如果 major值相等，则判断baseminor大于等于baseminor成员，或者大于最大baseminor+minorct
        */
        if (
            (*cp)->major > major ||
            ((*cp)->major == major && (((*cp)->baseminor >= baseminor) || ((*cp)->baseminor + (*cp)->minorct > baseminor)))
            )
            break;

    /* Check for overlapping minor ranges.  */
    if (*cp && (*cp)->major == major) {
        int old_min = (*cp)->baseminor;
        int old_max = (*cp)->baseminor + (*cp)->minorct - 1;
        int new_min = baseminor;
        int new_max = baseminor + minorct - 1;

        /* New driver overlaps from the left.  */
        /**
         * 最大新从id在旧的从id内，直接退出goto out
        */
        if (new_max >= old_min && new_max <= old_max) {
            ret = -EBUSY;
            goto out;
        }

        /* New driver overlaps from the right.  */
        /**
         * 最小新从id在旧的从id内，直接退出goto out
        */
        if (new_min <= old_max && new_min >= old_min) {
            ret = -EBUSY;
            goto out;
        }
    }

    /**
     * 将*cp赋值给新创建的char_device_struct（cd->next）的下一个
     * 
     * 再将char_device_struct（cd）赋值给*cp
     * 
     * 相当于链表插入
    */
    cd->next = *cp;
    *cp = cd;
    mutex_unlock(&chrdevs_lock);
    return cd;
out:
    mutex_unlock(&chrdevs_lock);
    kfree(cd);
    return ERR_PTR(ret);
}

static inline int major_to_index(unsigned major)                                                                                                                                                               
{
    //  fs/char_dev.c:32:#define CHRDEV_MAJOR_HASH_SIZE 255
    return major % CHRDEV_MAJOR_HASH_SIZE;
}


static int find_dynamic_major(void)                                                                                                                                                                            
{
    int i;
    struct char_device_struct *cd;

    /**
     *  include/linux/fs.h:2528:#define CHRDEV_MAJOR_DYN_END 234
     *  由于ARRAY_SIZE(chrdevs)得到255，从254到235
     * 
     *  在235到254之间，只要为空都可以使用，知己返回
    */
    for (i = ARRAY_SIZE(chrdevs)-1; i > CHRDEV_MAJOR_DYN_END; i--) {
        if (chrdevs[i] == NULL)
            return i;
    }

    //  include/linux/fs.h:2530:#define CHRDEV_MAJOR_DYN_EXT_START 511
    //  include/linux/fs.h:2531:#define CHRDEV_MAJOR_DYN_EXT_END 384
    /**
     * i取值为 511 到 385
    */
    for (i = CHRDEV_MAJOR_DYN_EXT_START; i > CHRDEV_MAJOR_DYN_EXT_END; i--) {
        /**
         * major_to_index （major % CHRDEV_MAJOR_HASH_SIZE） 取余数
         * chrdevs 的下标取值为 1,0，[254~103]
        */
        for (cd = chrdevs[major_to_index(i)]; cd; cd = cd->next)
            if (cd->major == i)
                break;

        /**
         * 要么cd == NULL，要么cd->major != i
        */
        if (cd == NULL || cd->major != i)
            return i;
    }

    return -EBUSY;
}



/**
 * cdev_init() - initialize a cdev structure
 * @cdev: the structure to initialize
 * @fops: the file_operations for this device
 *
 * Initializes @cdev, remembering @fops, making it ready to add to the
 * system with cdev_add().
 */
void cdev_init(struct cdev *cdev, const struct file_operations *fops)                                                                                                                                          
{
    memset(cdev, 0, sizeof *cdev);
    INIT_LIST_HEAD(&cdev->list);
    kobject_init(&cdev->kobj, &ktype_cdev_default);
    cdev->ops = fops;
}

//  @   include/linux/cdev.h
struct cdev {                                                                                                                                                                                                  
    struct kobject kobj;
    struct module *owner;
    const struct file_operations *ops;
    struct list_head list;
    dev_t dev;
    unsigned int count;
} __randomize_layout;

/**
 * 
*/
static struct kobj_type ktype_cdev_default = {                                                                                                                                                                 
    .release    = cdev_default_release,
};


///////////////////////////////////////////////////////////////////////////////////////
/**
 * cdev_add() - add a char device to the system
 * @p: the cdev structure for the device
 * @dev: the first device number for which this device is responsible
 * @count: the number of consecutive minor numbers corresponding to this
 *         device
 *
 * cdev_add() adds the device represented by @p to the system, making it
 * live immediately.  A negative error code is returned on failure.
 */
int cdev_add(struct cdev *p, dev_t dev, unsigned count)                                                                                                                                                        
{
    int error;

    p->dev = dev;
    p->count = count;

    /**
     * cdev_map 在 chrdev_init 中初始化
    */
    error = kobj_map(cdev_map, dev, count, NULL,exact_match, exact_lock, p);
    if (error)
        return error;

    /**
     * 增加计数
    */
    kobject_get(p->kobj.parent);

    return 0;
}

struct kobj_map {
	struct probe {
		struct probe *next;
		dev_t dev;
		unsigned long range;
		struct module *owner;
		kobj_probe_t *get;
		int (*lock)(dev_t, void *);
		void *data;
	} *probes[255];
	struct mutex *lock;
};

/**
 * data 为 cdev
*/
int kobj_map(struct kobj_map *domain, dev_t dev, unsigned long range,
	     struct module *module, kobj_probe_t *probe,
	     int (*lock)(dev_t, void *), void *data)
{
	unsigned n = MAJOR(dev + range - 1) - MAJOR(dev) + 1;
	unsigned index = MAJOR(dev);
	unsigned i;
	struct probe *p;

	if (n > 255)
		n = 255;

	p = kmalloc_array(n, sizeof(struct probe), GFP_KERNEL);
	if (p == NULL)
		return -ENOMEM;

	for (i = 0; i < n; i++, p++) {
		p->owner = module;
		p->get = probe;
		p->lock = lock;
		p->dev = dev;
		p->range = range;
		p->data = data;  //cdev
	}
	mutex_lock(domain->lock);
	for (i = 0, p -= n; i < n; i++, p++, index++) {
        /**
         * 在domain（kobj_map） 的成员probes(255个probe)中查找非空，且range大于当前range的probe
        */
		struct probe **s = &domain->probes[index % 255];
		while (*s && (*s)->range < range)
			s = &(*s)->next;
		p->next = *s;
		*s = p;
	}
	mutex_unlock(domain->lock);
	return 0;
}




/**
 * 调用堆栈信息
 * asmlinkage __visible void __init start_kernel(void)
 * --> void __init vfs_caches_init(void)
*/
void __init chrdev_init(void)
{
    /**
     * 
    */
    cdev_map = kobj_map_init(base_probe, &chrdevs_lock);                                                                                                                                                    
}

static struct kobject *base_probe(dev_t dev, int *part, void *data)
{
    /**
     * 
    */
    if (request_module("char-major-%d-%d", MAJOR(dev), MINOR(dev)) > 0)
        /* Make old-style 2.4 aliases work */
        request_module("char-major-%d", MAJOR(dev));
    return NULL;
}


struct kobj_map *kobj_map_init(kobj_probe_t *base_probe, struct mutex *lock)
{
    struct kobj_map *p = kmalloc(sizeof(struct kobj_map), GFP_KERNEL);
    struct probe *base = kzalloc(sizeof(*base), GFP_KERNEL);
    int i;

    if ((p == NULL) || (base == NULL)) {
        kfree(p);
        kfree(base);
        return NULL;
    }

    base->dev = 1;
    base->range = ~0;
    base->get = base_probe;
    /**
     * 每个p(kobj_map)有255个成员probe
     * 
    */
    for (i = 0; i < 255; i++)
        p->probes[i] = base;
    p->lock = lock;
    return p;
}





////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * device_add - add device to device hierarchy.
 * @dev: device.
 *
 * This is part 2 of device_register(), though may be called
 * separately _iff_ device_initialize() has been called separately.
 *
 * This adds @dev to the kobject hierarchy via kobject_add(), adds it
 * to the global and sibling lists for the device, then
 * adds it to the other relevant subsystems of the driver model.
 *
 * Do not call this routine or device_register() more than once for
 * any device structure.  The driver model core is not designed to work
 * with devices that get unregistered and then spring back to life.
 * (Among other things, it's very hard to guarantee that all references
 * to the previous incarnation of @dev have been dropped.)  Allocate
 * and register a fresh new struct device instead.
 *
 * NOTE: _Never_ directly free @dev after calling this function, even
 * if it returned an error! Always use put_device() to give up your
 * reference instead.
 */
int device_add(struct device *dev)
{
	struct device *parent;
	struct kobject *kobj;
	struct class_interface *class_intf;
	int error = -EINVAL;
	struct kobject *glue_dir = NULL;

	dev = get_device(dev);
	if (!dev)
		goto done;

	if (!dev->p) {
		error = device_private_init(dev);
		if (error)
			goto done;
	}

	/*
	 * for statically allocated devices, which should all be converted
	 * some day, we need to initialize the name. We prevent reading back
	 * the name, and force the use of dev_name()
	 */
	if (dev->init_name) {
		dev_set_name(dev, "%s", dev->init_name);
		dev->init_name = NULL;
	}

	/* subsystems can specify simple device enumeration */
	if (!dev_name(dev) && dev->bus && dev->bus->dev_name)
		dev_set_name(dev, "%s%u", dev->bus->dev_name, dev->id);

	if (!dev_name(dev)) {
		error = -EINVAL;
		goto name_error;
	}

	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);

	parent = get_device(dev->parent);
	kobj = get_device_parent(dev, parent);
	if (IS_ERR(kobj)) {
		error = PTR_ERR(kobj);
		goto parent_error;
	}
	if (kobj)
		dev->kobj.parent = kobj;

	/* use parent numa_node */
	if (parent && (dev_to_node(dev) == NUMA_NO_NODE))
		set_dev_node(dev, dev_to_node(parent));

	/* first, register with generic layer. */
	/* we require the name to be set before, and pass NULL */
	error = kobject_add(&dev->kobj, dev->kobj.parent, NULL);
	if (error) {
		glue_dir = get_glue_dir(dev);
		goto Error;
	}

	/* notify platform of device entry */
	if (platform_notify)
		platform_notify(dev);

	error = device_create_file(dev, &dev_attr_uevent);
	if (error)
		goto attrError;

	error = device_add_class_symlinks(dev);
	if (error)
		goto SymlinkError;
	error = device_add_attrs(dev);
	if (error)
		goto AttrsError;
	error = bus_add_device(dev);
	if (error)
		goto BusError;
	error = dpm_sysfs_add(dev);
	if (error)
		goto DPMError;
	device_pm_add(dev);

	if (MAJOR(dev->devt)) {
		error = device_create_file(dev, &dev_attr_dev);
		if (error)
			goto DevAttrError;

		error = device_create_sys_dev_entry(dev);
		if (error)
			goto SysEntryError;

		devtmpfs_create_node(dev);
	}

	/* Notify clients of device addition.  This call must come
	 * after dpm_sysfs_add() and before kobject_uevent().
	 */
	if (dev->bus)
		blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
					     BUS_NOTIFY_ADD_DEVICE, dev);

	kobject_uevent(&dev->kobj, KOBJ_ADD);
	bus_probe_device(dev);
	if (parent)
		klist_add_tail(&dev->p->knode_parent,
			       &parent->p->klist_children);

	if (dev->class) {
		mutex_lock(&dev->class->p->mutex);
		/* tie the class to the device */
		klist_add_tail(&dev->knode_class,
			       &dev->class->p->klist_devices);

		/* notify any interfaces that the device is here */
		list_for_each_entry(class_intf,
				    &dev->class->p->interfaces, node)
			if (class_intf->add_dev)
				class_intf->add_dev(dev, class_intf);
		mutex_unlock(&dev->class->p->mutex);
	}
done:
	put_device(dev);
	return error;
 SysEntryError:
	if (MAJOR(dev->devt))
		device_remove_file(dev, &dev_attr_dev);
 DevAttrError:
	device_pm_remove(dev);
	dpm_sysfs_remove(dev);
 DPMError:
	bus_remove_device(dev);
 BusError:
	device_remove_attrs(dev);
 AttrsError:
	device_remove_class_symlinks(dev);
 SymlinkError:
	device_remove_file(dev, &dev_attr_uevent);
 attrError:
	kobject_uevent(&dev->kobj, KOBJ_REMOVE);
	glue_dir = get_glue_dir(dev);
	kobject_del(&dev->kobj);
 Error:
	cleanup_glue_dir(dev, glue_dir);
parent_error:
	put_device(parent);
name_error:
	kfree(dev->p);
	dev->p = NULL;
	goto done;
}
EXPORT_SYMBOL_GPL(device_add);




/**
 * device_create - creates a device and registers it with sysfs
 * @class: pointer to the struct class that this device should be registered to
 * @parent: pointer to the parent struct device of this new device, if any
 * @devt: the dev_t for the char device to be added
 * @drvdata: the data to be added to the device for callbacks
 * @fmt: string for the device's name
 *
 * This function can be used by char device classes.  A struct device
 * will be created in sysfs, registered to the specified class.
 *
 * A "dev" file will be created, showing the dev_t for the device, if
 * the dev_t is not 0,0.
 * If a pointer to a parent struct device is passed in, the newly created
 * struct device will be a child of that device in sysfs.
 * The pointer to the struct device will be returned from the call.
 * Any further sysfs files that might be required can be created using this
 * pointer.
 *
 * Returns &struct device pointer on success, or ERR_PTR() on error.
 *
 * Note: the struct class passed to this function must have previously
 * been created with a call to class_create().
 */
struct device *device_create(struct class *class, struct device *parent,
			     dev_t devt, void *drvdata, const char *fmt, ...)
{
	va_list vargs;
	struct device *dev;

	va_start(vargs, fmt);
	dev = device_create_vargs(class, parent, devt, drvdata, fmt, vargs);
	va_end(vargs);
	return dev;
}
EXPORT_SYMBOL_GPL(device_create);

struct device *device_create_vargs(struct class *class, struct device *parent,
				   dev_t devt, void *drvdata, const char *fmt,
				   va_list args)
{
	return device_create_groups_vargs(class, parent, devt, drvdata, NULL,
					  fmt, args);
}
EXPORT_SYMBOL_GPL(device_create_vargs);

static struct device *
device_create_groups_vargs(struct class *class, struct device *parent,
			   dev_t devt, void *drvdata,
			   const struct attribute_group **groups,
			   const char *fmt, va_list args)
{
	struct device *dev = NULL;
	int retval = -ENODEV;

	if (class == NULL || IS_ERR(class))
		goto error;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		retval = -ENOMEM;
		goto error;
	}

    /**
     * 
    */
	device_initialize(dev);
	dev->devt = devt;
	dev->class = class;
	dev->parent = parent;
	dev->groups = groups;
	dev->release = device_create_release;
	dev_set_drvdata(dev, drvdata);

	retval = kobject_set_name_vargs(&dev->kobj, fmt, args);
	if (retval)
		goto error;

    /**
     * 
    */
	retval = device_add(dev);
	if (retval)
		goto error;

	return dev;

error:
	put_device(dev);
	return ERR_PTR(retval);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
static inline int register_chrdev(unsigned int major, const char *name,
                  const struct file_operations *fops)
{
    return __register_chrdev(major, 0, 256, name, fops);
}

/**
 * __register_chrdev() - create and register a cdev occupying a range of minors
 * @major: major device number or 0 for dynamic allocation
 * @baseminor: first of the requested range of minor numbers
 * @count: the number of minor numbers required
 * @name: name of this range of devices
 * @fops: file operations associated with this devices
 *
 * If @major == 0 this functions will dynamically allocate a major and return
 * its number.
 *
 * If @major > 0 this function will attempt to reserve a device with the given
 * major number and will return zero on success.
 *
 * Returns a -ve errno on failure.
 *
 * The name of this device has nothing to do with the name of the device in
 * /dev. It only helps to keep track of the different owners of devices. If
 * your module name has only one type of devices it's ok to use e.g. the name
 * of the module here.
 */
int __register_chrdev(unsigned int major, unsigned int baseminor,
              unsigned int count, const char *name,
              const struct file_operations *fops)
{
    struct char_device_struct *cd;
    struct cdev *cdev;
    int err = -ENOMEM;

    cd = __register_chrdev_region(major, baseminor, count, name);
    if (IS_ERR(cd))
        return PTR_ERR(cd);

    cdev = cdev_alloc();
    if (!cdev)
        goto out2;

    cdev->owner = fops->owner;
    cdev->ops = fops;
    kobject_set_name(&cdev->kobj, "%s", name);

    err = cdev_add(cdev, MKDEV(cd->major, baseminor), count);
    if (err)
        goto out;

    cd->cdev = cdev;

    return major ? 0 : cd->major;
out:
    kobject_put(&cdev->kobj);
out2:
    kfree(__unregister_chrdev_region(cd->major, baseminor, count));
    return err;
}

