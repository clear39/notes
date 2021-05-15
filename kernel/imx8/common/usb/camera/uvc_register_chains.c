
// @ drivers/media/usb/uvc/uvc_driver.c
static int uvc_register_chains(struct uvc_device *dev)
{
	struct uvc_video_chain *chain;
	int ret;

	list_for_each_entry(chain, &dev->chains, list) {
		ret = uvc_register_terms(dev, chain);
		if (ret < 0)
			return ret;

#ifdef CONFIG_MEDIA_CONTROLLER
		ret = uvc_mc_register_entities(chain);
		if (ret < 0) {
			uvc_printk(KERN_INFO, "Failed to register entites " "(%d).\n", ret);
		}
#endif
	}

	return 0;
}


/*
 * Register all video devices in all chains.
 */
static int uvc_register_terms(struct uvc_device *dev, struct uvc_video_chain *chain)
{
	struct uvc_streaming *stream;
	struct uvc_entity *term;
	int ret;

	list_for_each_entry(term, &chain->entities, chain) {
		if (UVC_ENTITY_TYPE(term) != UVC_TT_STREAMING)
			continue;

		stream = uvc_stream_by_id(dev, term->id);
		if (stream == NULL) {
			uvc_printk(KERN_INFO, "No streaming interface found "
				   "for terminal %u.", term->id);
			continue;
		}

		stream->chain = chain;
		// video节点在这里进行注册
		ret = uvc_register_video(dev, stream);
		if (ret < 0)
			return ret;

		term->vdev = &stream->vdev;
	}

	return 0;
}


static struct uvc_streaming *uvc_stream_by_id(struct uvc_device *dev, int id)
{
	struct uvc_streaming *stream;

	list_for_each_entry(stream, &dev->streams, list) {
		if (stream->header.bTerminalLink == id)
			return stream;
	}

	return NULL;
}


static int uvc_register_video(struct uvc_device *dev, struct uvc_streaming *stream)
{
	struct video_device *vdev = &stream->vdev;
	int ret;
	printk("xqli uvc_register_video");
	/* Initialize the video buffers queue. */
	ret = uvc_queue_init(&stream->queue, stream->type, !uvc_no_drop_param);
	if (ret)
		return ret;

	/* Initialize the streaming interface with default streaming
	 * parameters.
	 */
	ret = uvc_video_init(stream);
	if (ret < 0) {
		uvc_printk(KERN_ERR, "Failed to initialize the device " "(%d).\n", ret);
		return ret;
	}

	uvc_debugfs_init_stream(stream);

	/* Register the device with V4L. */

	/* We already hold a reference to dev->udev. The video device will be
	 * unregistered before the reference is released, so we don't need to
	 * get another one.
	 */
	vdev->v4l2_dev = &dev->vdev;
	vdev->fops = &uvc_fops;
	vdev->ioctl_ops = &uvc_ioctl_ops;
	vdev->release = uvc_release;
	vdev->prio = &stream->chain->prio;
	if (stream->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		vdev->vfl_dir = VFL_DIR_TX;

	strlcpy(vdev->name, dev->name, sizeof vdev->name);

	/* Set the driver data before calling video_register_device, otherwise
	 * uvc_v4l2_open might race us.
	 */
	video_set_drvdata(vdev, stream);

	ret = video_register_device(vdev, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		uvc_printk(KERN_ERR, "Failed to register video device (%d).\n",
			   ret);
		return ret;
	}

	if (stream->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		stream->chain->caps |= V4L2_CAP_VIDEO_CAPTURE;
	else
		stream->chain->caps |= V4L2_CAP_VIDEO_OUTPUT;

	kref_get(&dev->ref);
	return 0;
}



int uvc_mc_register_entities(struct uvc_video_chain *chain)
{
	struct uvc_entity *entity;
	int ret;

	list_for_each_entry(entity, &chain->entities, chain) {
		ret = uvc_mc_init_entity(chain, entity);
		if (ret < 0) {
			uvc_printk(KERN_INFO, "Failed to initialize entity for "
				   "entity %u\n", entity->id);
			return ret;
		}
	}

	list_for_each_entry(entity, &chain->entities, chain) {
		ret = uvc_mc_create_links(chain, entity);
		if (ret < 0) {
			uvc_printk(KERN_INFO, "Failed to create links for "
				   "entity %u\n", entity->id);
			return ret;
		}
	}

	return 0;
}















