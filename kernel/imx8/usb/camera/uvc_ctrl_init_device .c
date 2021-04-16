// @ drivers/media/usb/uvc/uvc_ctrl.c
/*
 * Initialize device controls.
 */
int uvc_ctrl_init_device(struct uvc_device *dev)
{
	struct uvc_entity *entity;
	unsigned int i;
	// dev->entities 是在 uvc_parse_control中初始化
	/* Walk the entities list and instantiate controls */
	list_for_each_entry(entity, &dev->entities, list) {
		struct uvc_control *ctrl;
		unsigned int bControlSize = 0, ncontrols;
		__u8 *bmControls = NULL;
		
		//entity包含该多种格式信息
		if (UVC_ENTITY_TYPE(entity) == UVC_VC_EXTENSION_UNIT) {
			bmControls = entity->extension.bmControls;
			bControlSize = entity->extension.bControlSize;
		} else if (UVC_ENTITY_TYPE(entity) == UVC_VC_PROCESSING_UNIT) {
			bmControls = entity->processing.bmControls;
			bControlSize = entity->processing.bControlSize;
		} else if (UVC_ENTITY_TYPE(entity) == UVC_ITT_CAMERA) {
			bmControls = entity->camera.bmControls;
			bControlSize = entity->camera.bControlSize;
		}

		/* Remove bogus/blacklisted controls */
		uvc_ctrl_prune_entity(dev, entity);

		/* Count supported controls and allocate the controls array */
		ncontrols = memweight(bmControls, bControlSize);
		if (ncontrols == 0)
			continue;

		entity->controls = kcalloc(ncontrols, sizeof(*ctrl),GFP_KERNEL);
		if (entity->controls == NULL)
			return -ENOMEM;
		entity->ncontrols = ncontrols;

		/* Initialize all supported controls */
		ctrl = entity->controls;
		for (i = 0; i < bControlSize * 8; ++i) {
			if (uvc_test_bit(bmControls, i) == 0)
				continue;

			ctrl->entity = entity;
			ctrl->index = i;

			uvc_ctrl_init_ctrl(dev, ctrl);
			ctrl++;
		}
	}

	return 0;
}


/*
 * Prune an entity of its bogus controls using a blacklist. Bogus controls
 * are currently the ones that crash the camera or unconditionally return an
 * error when queried.
 */
static void uvc_ctrl_prune_entity(struct uvc_device *dev,
	struct uvc_entity *entity)
{
	struct uvc_ctrl_blacklist {
		struct usb_device_id id;
		u8 index;
	};

	static const struct uvc_ctrl_blacklist processing_blacklist[] = {
		{ { USB_DEVICE(0x13d3, 0x509b) }, 9 }, /* Gain */
		{ { USB_DEVICE(0x1c4f, 0x3000) }, 6 }, /* WB Temperature */
		{ { USB_DEVICE(0x5986, 0x0241) }, 2 }, /* Hue */
	};
	static const struct uvc_ctrl_blacklist camera_blacklist[] = {
		{ { USB_DEVICE(0x06f8, 0x3005) }, 9 }, /* Zoom, Absolute */
	};

	const struct uvc_ctrl_blacklist *blacklist;
	unsigned int size;
	unsigned int count;
	unsigned int i;
	u8 *controls;

	switch (UVC_ENTITY_TYPE(entity)) {
	case UVC_VC_PROCESSING_UNIT:
		blacklist = processing_blacklist;
		count = ARRAY_SIZE(processing_blacklist);
		controls = entity->processing.bmControls;
		size = entity->processing.bControlSize;
		break;

	case UVC_ITT_CAMERA:
		blacklist = camera_blacklist;
		count = ARRAY_SIZE(camera_blacklist);
		controls = entity->camera.bmControls;
		size = entity->camera.bControlSize;
		break;

	default:
		return;
	}

	for (i = 0; i < count; ++i) {
		if (!usb_match_one_id(dev->intf, &blacklist[i].id))
			continue;

		if (blacklist[i].index >= 8 * size ||
		    !uvc_test_bit(controls, blacklist[i].index))
			continue;

		uvc_trace(UVC_TRACE_CONTROL, "%u/%u control is black listed, "
			"removing it.\n", entity->id, blacklist[i].index);

		uvc_clear_bit(controls, blacklist[i].index);
	}
}


/*
 * Add control information and hardcoded stock control mappings to the given
 * device.
 */
static void uvc_ctrl_init_ctrl(struct uvc_device *dev, struct uvc_control *ctrl)
{
	const struct uvc_control_info *info = uvc_ctrls;
	const struct uvc_control_info *iend = info + ARRAY_SIZE(uvc_ctrls);
	const struct uvc_control_mapping *mapping = uvc_ctrl_mappings;
	const struct uvc_control_mapping *mend =
		mapping + ARRAY_SIZE(uvc_ctrl_mappings);

	/* XU controls initialization requires querying the device for control
	 * information. As some buggy UVC devices will crash when queried
	 * repeatedly in a tight loop, delay XU controls initialization until
	 * first use.
	 */
	if (UVC_ENTITY_TYPE(ctrl->entity) == UVC_VC_EXTENSION_UNIT)
		return;

	for (; info < iend; ++info) {
		if (uvc_entity_match_guid(ctrl->entity, info->entity) &&
		    ctrl->index == info->index) {
			uvc_ctrl_add_info(dev, ctrl, info);
			break;
		 }
	}

	if (!ctrl->initialized)
		return;

	for (; mapping < mend; ++mapping) {
		if (uvc_entity_match_guid(ctrl->entity, mapping->entity) &&
		    ctrl->info.selector == mapping->selector)
			__uvc_ctrl_add_mapping(dev, ctrl, mapping);
	}
}

