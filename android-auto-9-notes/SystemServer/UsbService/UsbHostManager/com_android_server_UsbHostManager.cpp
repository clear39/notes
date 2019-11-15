
//  @   frameworks/base/services/core/jni/com_android_server_UsbHostManager.cpp
static void android_server_UsbHostManager_monitorUsbHostBus(JNIEnv* /* env */, jobject thiz)
{
    /**
     * @    system/core/libusbhost/usbhost.c
    */
    struct usb_host_context* context = usb_host_init();
    if (!context) {
        ALOGE("usb_host_init failed");
        return;
    }
    // this will never return so it is safe to pass thiz directly
    usb_host_run(context, usb_device_added, usb_device_removed, NULL, (void *)thiz);
}


struct usb_host_context *usb_host_init()
{
    struct usb_host_context *context = calloc(1, sizeof(struct usb_host_context));
    if (!context) {
        fprintf(stderr, "out of memory in usb_host_context\n");
        return NULL;
    }
    context->fd = inotify_init();
    if (context->fd < 0) {
        fprintf(stderr, "inotify_init failed\n");
        free(context);
        return NULL;
    }
    return context;
}


void usb_host_run(struct usb_host_context *context,
                  usb_device_added_cb added_cb,
                  usb_device_removed_cb removed_cb,
                  usb_discovery_done_cb discovery_done_cb,
                  void *client_data)
{
    int done;

    done = usb_host_load(context, added_cb, removed_cb, discovery_done_cb, client_data);

    while (!done) {

        done = usb_host_read_event(context);
    }
} /* usb_host_run() */



int usb_host_load(struct usb_host_context *context,
                  usb_device_added_cb added_cb,
                  usb_device_removed_cb removed_cb,
                  usb_discovery_done_cb discovery_done_cb,
                  void *client_data)
{
    int done = 0;
    int i;

    context->cb_added = added_cb;
    context->cb_removed = removed_cb;
    context->data = client_data;

    D("Created device discovery thread\n");

    /* watch for files added and deleted within USB_FS_DIR */
    context->wddbus = -1;
    /**
     * #define MAX_USBFS_WD_COUNT      10
    */
    for (i = 0; i < MAX_USBFS_WD_COUNT; i++)
        context->wds[i] = -1;

    /**
     *  watch the root for new subdirectories 
     * #define DEV_DIR             "/dev"
     * 
     * */
    context->wdd = inotify_add_watch(context->fd, DEV_DIR, IN_CREATE | IN_DELETE);
    if (context->wdd < 0) {
        fprintf(stderr, "inotify_add_watch failed\n");
        if (discovery_done_cb)
            discovery_done_cb(client_data);
        return done;
    }
    /**
     * #define MAX_USBFS_WD_COUNT      10
    */
    watch_existing_subdirs(context, context->wds, MAX_USBFS_WD_COUNT);

    /* check for existing devices first, after we have inotify set up */
    done = find_existing_devices(added_cb, client_data);
    if (discovery_done_cb)
        done |= discovery_done_cb(client_data);

    return done;
} /* usb_host_load() */


static void watch_existing_subdirs(struct usb_host_context *context,int *wds, int wd_count)
{
    char path[100];
    int i, ret;
    /**
     * #define DEV_DIR             "/dev"
     * #define DEV_BUS_DIR         DEV_DIR "/bus"
     * #define USB_FS_DIR          DEV_BUS_DIR "/usb"
     * 
     * USB_FS_DIR = "/dev/bus/usb"
    */
    wds[0] = inotify_add_watch(context->fd, USB_FS_DIR, IN_CREATE | IN_DELETE);
    if (wds[0] < 0)
        return;

    /* watch existing subdirectories of USB_FS_DIR */
    for (i = 1; i < wd_count; i++) {
        /**
         * USB_FS_DIR = "/dev/bus/usb(1~9)"
        */
        snprintf(path, sizeof(path), USB_FS_DIR "/%03d", i);
        ret = inotify_add_watch(context->fd, path, IN_CREATE | IN_DELETE);
        if (ret >= 0)
            wds[i] = ret;
    }
}



/* returns true if one of the callbacks indicates we are done */
static int find_existing_devices(usb_device_added_cb added_cb,
                                  void *client_data)
{
    char busname[32];
    DIR *busdir;
    struct dirent *de;
    int done = 0;

    /**
     * USB_FS_DIR = "/dev/bus/usb"
    */
    busdir = opendir(USB_FS_DIR);
    if(busdir == 0) return 0;

    while ((de = readdir(busdir)) != 0 && !done) {
        if(badname(de->d_name)) continue;

        snprintf(busname, sizeof(busname), USB_FS_DIR "/%s", de->d_name);
        done = find_existing_devices_bus(busname, added_cb,client_data);
    } //end of busdir while
    closedir(busdir);

    return done;
}


static int find_existing_devices_bus(char *busname,usb_device_added_cb added_cb,void *client_data)
{
    char devname[32];
    DIR *devdir;
    struct dirent *de;
    int done = 0;

    devdir = opendir(busname);
    if(devdir == 0) return 0;

    while ((de = readdir(devdir)) && !done) {
        if(badname(de->d_name)) continue;

        snprintf(devname, sizeof(devname), "%s/%s", busname, de->d_name);
        done = added_cb(devname, client_data);
    } // end of devdir while
    closedir(devdir);

    return done;
}



int usb_host_read_event(struct usb_host_context *context)
{
    struct inotify_event* event;
    char event_buf[512];
    char path[100];
    int i, ret, done = 0;
    int offset = 0;
    int wd;

    ret = read(context->fd, event_buf, sizeof(event_buf));
    if (ret >= (int)sizeof(struct inotify_event)) {
        while (offset < ret && !done) {
            event = (struct inotify_event*)&event_buf[offset];
            done = 0;
            wd = event->wd;
            if (wd == context->wdd) {
                if ((event->mask & IN_CREATE) && !strcmp(event->name, "bus")) {
                    context->wddbus = inotify_add_watch(context->fd, DEV_BUS_DIR, IN_CREATE | IN_DELETE);
                    if (context->wddbus < 0) {
                        done = 1;
                    } else {
                        watch_existing_subdirs(context, context->wds, MAX_USBFS_WD_COUNT);
                        done = find_existing_devices(context->cb_added, context->data);
                    }
                }
            } else if (wd == context->wddbus) {
                if ((event->mask & IN_CREATE) && !strcmp(event->name, "usb")) {
                    watch_existing_subdirs(context, context->wds, MAX_USBFS_WD_COUNT);
                    done = find_existing_devices(context->cb_added, context->data);
                } else if ((event->mask & IN_DELETE) && !strcmp(event->name, "usb")) {
                    for (i = 0; i < MAX_USBFS_WD_COUNT; i++) {
                        if (context->wds[i] >= 0) {
                            inotify_rm_watch(context->fd, context->wds[i]);
                            context->wds[i] = -1;
                        }
                    }
                }
            } else if (wd == context->wds[0]) {
                i = atoi(event->name);
                snprintf(path, sizeof(path), USB_FS_DIR "/%s", event->name);
                D("%s subdirectory %s: index: %d\n", (event->mask & IN_CREATE) ?
                        "new" : "gone", path, i);
                if (i > 0 && i < MAX_USBFS_WD_COUNT) {
                    int local_ret = 0;
                    if (event->mask & IN_CREATE) {
                        local_ret = inotify_add_watch(context->fd, path,
                                IN_CREATE | IN_DELETE);
                        if (local_ret >= 0)
                            context->wds[i] = local_ret;
                        done = find_existing_devices_bus(path, context->cb_added,
                                context->data);
                    } else if (event->mask & IN_DELETE) {
                        inotify_rm_watch(context->fd, context->wds[i]);
                        context->wds[i] = -1;
                    }
                }
            } else {
                for (i = 1; (i < MAX_USBFS_WD_COUNT) && !done; i++) {
                    if (wd == context->wds[i]) {
                        snprintf(path, sizeof(path), USB_FS_DIR "/%03d/%s", i, event->name);
                        if (event->mask == IN_CREATE) {
                            D("new device %s\n", path);
                            done = context->cb_added(path, context->data);
                        } else if (event->mask == IN_DELETE) {
                            D("gone device %s\n", path);
                            done = context->cb_removed(path, context->data);
                        }
                    }
                }
            }

            offset += sizeof(struct inotify_event) + event->len;
        }
    }

    return done;
} /* usb_host_read_event() */
