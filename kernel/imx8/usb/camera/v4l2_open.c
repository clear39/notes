// @ drivers/media/v4l2-core/v4l2-dev.c
/* Override for the open function */
static int v4l2_open(struct inode *inode, struct file *filp)
{
	struct video_device *vdev;
	int ret = 0;

	/* Check if the video device is available */
	mutex_lock(&videodev_lock);
	vdev = video_devdata(filp);
	/* return ENODEV if the video device has already been removed. */
	if (vdev == NULL || !video_is_registered(vdev)) {
		mutex_unlock(&videodev_lock);
		return -ENODEV;
	}
	/* and increase the device refcount */
	video_get(vdev);
	mutex_unlock(&videodev_lock);
	if (vdev->fops->open) {
		if (video_is_registered(vdev))
			ret = vdev->fops->open(filp);
		else
			ret = -ENODEV;
	}

	if (vdev->dev_debug & V4L2_DEV_DEBUG_FOP)
		printk(KERN_DEBUG "%s: open (%d)\n", video_device_node_name(vdev), ret);
	/* decrease the refcount in case of an error */
	if (ret)
		video_put(vdev);
	return ret;
}


static ssize_t v4l2_read(struct file *filp, char __user *buf, size_t sz, loff_t *off)
{
	struct video_device *vdev = video_devdata(filp);
	int ret = -ENODEV;

	if (!vdev->fops->read)
		return -EINVAL;
	if (video_is_registered(vdev))
		ret = vdev->fops->read(filp, buf, sz, off);
	if ((vdev->dev_debug & V4L2_DEV_DEBUG_FOP) && (vdev->dev_debug & V4L2_DEV_DEBUG_STREAMING))
		printk(KERN_DEBUG "%s: read: %zd (%d)\n", video_device_node_name(vdev), sz, ret);
	return ret;
}

static ssize_t v4l2_write(struct file *filp, const char __user *buf, size_t sz, loff_t *off)
{
	struct video_device *vdev = video_devdata(filp);
	int ret = -ENODEV;

	if (!vdev->fops->write)
		return -EINVAL;
	if (video_is_registered(vdev))
		ret = vdev->fops->write(filp, buf, sz, off);
	if ((vdev->dev_debug & V4L2_DEV_DEBUG_FOP) && (vdev->dev_debug & V4L2_DEV_DEBUG_STREAMING))
		printk(KERN_DEBUG "%s: write: %zd (%d)\n", video_device_node_name(vdev), sz, ret);
	return ret;
}


static unsigned int v4l2_poll(struct file *filp, struct poll_table_struct *poll)
{
	struct video_device *vdev = video_devdata(filp);
	unsigned int res = POLLERR | POLLHUP;

	if (!vdev->fops->poll)
		return DEFAULT_POLLMASK;
	if (video_is_registered(vdev))
		res = vdev->fops->poll(filp, poll);
	if (vdev->dev_debug & V4L2_DEV_DEBUG_POLL)
		printk(KERN_DEBUG "%s: poll: %08x\n", video_device_node_name(vdev), res);
	return res;
}



static long v4l2_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct video_device *vdev = video_devdata(filp);
	int ret = -ENODEV;

	if (vdev->fops->unlocked_ioctl) {
		struct mutex *lock = v4l2_ioctl_get_lock(vdev, cmd);

		if (lock && mutex_lock_interruptible(lock))
			return -ERESTARTSYS;
		if (video_is_registered(vdev))
			ret = vdev->fops->unlocked_ioctl(filp, cmd, arg);
		if (lock)
			mutex_unlock(lock);
	} else if (vdev->fops->ioctl) {
		/* This code path is a replacement for the BKL. It is a major
		 * hack but it will have to do for those drivers that are not
		 * yet converted to use unlocked_ioctl.
		 *
		 * All drivers implement struct v4l2_device, so we use the
		 * lock defined there to serialize the ioctls.
		 *
		 * However, if the driver sleeps, then it blocks all ioctls
		 * since the lock is still held. This is very common for
		 * VIDIOC_DQBUF since that normally waits for a frame to arrive.
		 * As a result any other ioctl calls will proceed very, very
		 * slowly since each call will have to wait for the VIDIOC_QBUF
		 * to finish. Things that should take 0.01s may now take 10-20
		 * seconds.
		 *
		 * The workaround is to *not* take the lock for VIDIOC_DQBUF.
		 * This actually works OK for videobuf-based drivers, since
		 * videobuf will take its own internal lock.
		 */
		struct mutex *m = &vdev->v4l2_dev->ioctl_lock;

		if (cmd != VIDIOC_DQBUF && mutex_lock_interruptible(m))
			return -ERESTARTSYS;
		if (video_is_registered(vdev))
			ret = vdev->fops->ioctl(filp, cmd, arg);
		if (cmd != VIDIOC_DQBUF)
			mutex_unlock(m);
	} else
		ret = -ENOTTY;

	return ret;
}








