// dump 命令
int CommandListener::DumpCmd::runCommand(SocketClient *cli,int argc, char **argv) {
    cli->sendMsg(0, "Dumping loop status", false);
    if (Loop::dumpState(cli)) {
        cli->sendMsg(ResponseCode::CommandOkay, "Loop dump failed", true);
    }
    cli->sendMsg(0, "Dumping DM status", false);
    if (Devmapper::dumpState(cli)) {
        cli->sendMsg(ResponseCode::CommandOkay, "Devmapper dump failed", true);
    }
    cli->sendMsg(0, "Dumping mounted filesystems", false);
    FILE *fp = fopen("/proc/mounts", "r");
    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            line[strlen(line)-1] = '\0';
            cli->sendMsg(0, line, false);;
        }
        fclose(fp);
    }

    cli->sendMsg(ResponseCode::CommandOkay, "dump complete", false);
    return 0;
}


int Loop::dumpState(SocketClient *c) {
    int i;
    int fd;
    char filename[256];

    for (i = 0; i < LOOP_MAX; i++) { // static const int LOOP_MAX = 4096;
        struct loop_info64 li;
        int rc;

        sprintf(filename, "/dev/block/loop%d", i);

        if ((fd = open(filename, O_RDWR)) < 0) {
            if (errno != ENOENT) {
                SLOGE("Unable to open %s (%s)", filename, strerror(errno));
            } else {
                continue;
            }
            return -1;
        }

        rc = ioctl(fd, LOOP_GET_STATUS64, &li);
        close(fd);
        if (rc < 0 && errno == ENXIO) {
            continue;
        }

        if (rc < 0) {
            SLOGE("Unable to get loop status for %s (%s)", filename,strerror(errno));
            return -1;
        }
        char *tmp = NULL;
        asprintf(&tmp, "%s %d %lld:%lld %llu %lld:%lld %lld 0x%x {%s} {%s}", filename, li.lo_number,
                MAJOR(li.lo_device), MINOR(li.lo_device), li.lo_inode, MAJOR(li.lo_rdevice),
                        MINOR(li.lo_rdevice), li.lo_offset, li.lo_flags, li.lo_crypt_name,li.lo_file_name);
        c->sendMsg(0, tmp, false);
        free(tmp);
    }
    return 0;
}




int Devmapper::dumpState(SocketClient *c) {

    char *buffer = (char *) malloc(1024 * 64);
    if (!buffer) {
        SLOGE("Error allocating memory (%s)", strerror(errno));
        return -1;
    }
    memset(buffer, 0, (1024 * 64));

    char *buffer2 = (char *) malloc(DEVMAPPER_BUFFER_SIZE); //#define DEVMAPPER_BUFFER_SIZE 4096
    if (!buffer2) {
        SLOGE("Error allocating memory (%s)", strerror(errno));
        free(buffer);
        return -1;
    }

    int fd;
    if ((fd = open("/dev/device-mapper", O_RDWR)) < 0) {
        SLOGE("Error opening devmapper (%s)", strerror(errno));
        free(buffer);
        free(buffer2);
        return -1;
    }

    struct dm_ioctl *io = (struct dm_ioctl *) buffer;
    ioctlInit(io, (1024 * 64), NULL, 0);

    if (ioctl(fd, DM_LIST_DEVICES, io)) {
        SLOGE("DM_LIST_DEVICES ioctl failed (%s)", strerror(errno));
        free(buffer);
        free(buffer2);
        close(fd);
        return -1;
    }

    struct dm_name_list *n = (struct dm_name_list *) (((char *) buffer) + io->data_start);
    if (!n->dev) {
        free(buffer);
        free(buffer2);
        close(fd);
        return 0;
    }

    unsigned nxt = 0;
    do {
        n = (struct dm_name_list *) (((char *) n) + nxt);

        memset(buffer2, 0, DEVMAPPER_BUFFER_SIZE);
        struct dm_ioctl *io2 = (struct dm_ioctl *) buffer2;
        ioctlInit(io2, DEVMAPPER_BUFFER_SIZE, n->name, 0);
        if (ioctl(fd, DM_DEV_STATUS, io2)) {
            if (errno != ENXIO) {
                SLOGE("DM_DEV_STATUS ioctl failed (%s)", strerror(errno));
            }
            io2 = NULL;
        }

        char *tmp;
        if (!io2) {
            asprintf(&tmp, "%s %llu:%llu (no status available)", n->name, MAJOR(n->dev), MINOR(n->dev));
        } else {
            asprintf(&tmp, "%s %llu:%llu %d %d 0x%.8x %llu:%llu", n->name, MAJOR(n->dev),
                    MINOR(n->dev), io2->target_count, io2->open_count, io2->flags, MAJOR(io2->dev),
                            MINOR(io2->dev));
        }
        c->sendMsg(0, tmp, false);
        free(tmp);
        nxt = n->next;
    } while (nxt);

    free(buffer);
    free(buffer2);
    close(fd);
    return 0;
}

