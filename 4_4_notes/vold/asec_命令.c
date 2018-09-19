int CommandListener::AsecCmd::runCommand(SocketClient *cli,int argc, char **argv) {
    if (argc < 2) {
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Missing Argument", false);
        return 0;
    }

    VolumeManager *vm = VolumeManager::Instance();
    int rc = 0;

    if (!strcmp(argv[1], "list")) {
        dumpArgs(argc, argv, -1);

        listAsecsInDirectory(cli, Volume::SEC_ASECDIR_EXT);
        listAsecsInDirectory(cli, Volume::SEC_ASECDIR_INT);
    } else if (!strcmp(argv[1], "create")) {
        dumpArgs(argc, argv, 5);
        if (argc != 8) {
            cli->sendMsg(ResponseCode::CommandSyntaxError,
                    "Usage: asec create <container-id> <size_mb> <fstype> <key> <ownerUid> "
                    "<isExternal>", false);
            return 0;
        }

        unsigned int numSectors = (atoi(argv[3]) * (1024 * 1024)) / 512;
        const bool isExternal = (atoi(argv[7]) == 1);
        rc = vm->createAsec(argv[2], numSectors, argv[4], argv[5], atoi(argv[6]), isExternal);
    } else if (!strcmp(argv[1], "finalize")) {
        dumpArgs(argc, argv, -1);
        if (argc != 3) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: asec finalize <container-id>", false);
            return 0;
        }
        rc = vm->finalizeAsec(argv[2]);
    } else if (!strcmp(argv[1], "fixperms")) {
        dumpArgs(argc, argv, -1);
        if  (argc != 5) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: asec fixperms <container-id> <gid> <filename>", false);
            return 0;
        }

        char *endptr;
        gid_t gid = (gid_t) strtoul(argv[3], &endptr, 10);
        if (*endptr != '\0') {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: asec fixperms <container-id> <gid> <filename>", false);
            return 0;
        }

        rc = vm->fixupAsecPermissions(argv[2], gid, argv[4]);
    } else if (!strcmp(argv[1], "destroy")) {
        dumpArgs(argc, argv, -1);
        if (argc < 3) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: asec destroy <container-id> [force]", false);
            return 0;
        }
        bool force = false;
        if (argc > 3 && !strcmp(argv[3], "force")) {
            force = true;
        }
        rc = vm->destroyAsec(argv[2], force);
    } else if (!strcmp(argv[1], "mount")) {
        dumpArgs(argc, argv, 3);
        if (argc != 5) {
            cli->sendMsg(ResponseCode::CommandSyntaxError,
                    "Usage: asec mount <namespace-id> <key> <ownerUid>", false);
            return 0;
        }
        rc = vm->mountAsec(argv[2], argv[3], atoi(argv[4]));
    } else if (!strcmp(argv[1], "unmount")) {
        dumpArgs(argc, argv, -1);
        if (argc < 3) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: asec unmount <container-id> [force]", false);
            return 0;
        }
        bool force = false;
        if (argc > 3 && !strcmp(argv[3], "force")) {
            force = true;
        }
        rc = vm->unmountAsec(argv[2], force);
    } else if (!strcmp(argv[1], "rename")) {
        dumpArgs(argc, argv, -1);
        if (argc != 4) {
            cli->sendMsg(ResponseCode::CommandSyntaxError,
                    "Usage: asec rename <old_id> <new_id>", false);
            return 0;
        }
        rc = vm->renameAsec(argv[2], argv[3]);
    } else if (!strcmp(argv[1], "path")) {
        dumpArgs(argc, argv, -1);
        if (argc != 3) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: asec path <container-id>", false);
            return 0;
        }
        char path[255];

        if (!(rc = vm->getAsecMountPath(argv[2], path, sizeof(path)))) {
            cli->sendMsg(ResponseCode::AsecPathResult, path, false);
            return 0;
        }
    } else if (!strcmp(argv[1], "fspath")) {
        dumpArgs(argc, argv, -1);
        if (argc != 3) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: asec fspath <container-id>", false);
            return 0;
        }
        char path[255];

        if (!(rc = vm->getAsecFilesystemPath(argv[2], path, sizeof(path)))) {
            cli->sendMsg(ResponseCode::AsecPathResult, path, false);
            return 0;
        }
    } else {
        dumpArgs(argc, argv, -1);
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Unknown asec cmd", false);
    }

    if (!rc) {
        cli->sendMsg(ResponseCode::CommandOkay, "asec operation succeeded", false);
    } else {
        rc = ResponseCode::convertFromErrno();
        cli->sendMsg(rc, "asec operation failed", true);
    }

    return 0;
}
