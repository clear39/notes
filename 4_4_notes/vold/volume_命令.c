int CommandListener::VolumeCmd::runCommand(SocketClient *cli,int argc, char **argv) {
    dumpArgs(argc, argv, -1);

    if (argc < 2) {
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Missing Argument", false);
        return 0;
    }

    VolumeManager *vm = VolumeManager::Instance();
    int rc = 0;

    if (!strcmp(argv[1], "list")) {
        return vm->listVolumes(cli);
    } else if (!strcmp(argv[1], "debug")) {
        if (argc != 3 || (argc == 3 && (strcmp(argv[2], "off") && strcmp(argv[2], "on")))) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: volume debug <off/on>", false);
            return 0;
        }
        vm->setDebug(!strcmp(argv[2], "on") ? true : false);
    } else if (!strcmp(argv[1], "mount")) {
        if (argc != 3) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: volume mount <path>", false);
            return 0;
        }
        rc = vm->mountVolume(argv[2]);
    } else if (!strcmp(argv[1], "unmount")) {
        if (argc < 3 || argc > 4 ||
           ((argc == 4 && strcmp(argv[3], "force")) &&
            (argc == 4 && strcmp(argv[3], "force_and_revert")))) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: volume unmount <path> [force|force_and_revert]", false);
            return 0;
        }

        bool force = false;
        bool revert = false;
        if (argc >= 4 && !strcmp(argv[3], "force")) {
            force = true;
        } else if (argc >= 4 && !strcmp(argv[3], "force_and_revert")) {
            force = true;
            revert = true;
        }
        rc = vm->unmountVolume(argv[2], force, revert);
    } else if (!strcmp(argv[1], "format")) {
        if (argc < 3 || argc > 4 ||
            (argc == 4 && strcmp(argv[3], "wipe"))) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: volume format <path> [wipe]", false);
            return 0;
        }
        bool wipe = false;
        if (argc >= 4 && !strcmp(argv[3], "wipe")) {
            wipe = true;
        }
        rc = vm->formatVolume(argv[2], wipe);
    } else if (!strcmp(argv[1], "share")) {
        if (argc != 4) {
            cli->sendMsg(ResponseCode::CommandSyntaxError,"Usage: volume share <path> <method>", false);
            return 0;
        }
        rc = vm->shareVolume(argv[2], argv[3]);
    } else if (!strcmp(argv[1], "unshare")) {
        if (argc != 4) {
            cli->sendMsg(ResponseCode::CommandSyntaxError,"Usage: volume unshare <path> <method>", false);
            return 0;
        }
        rc = vm->unshareVolume(argv[2], argv[3]);
    } else if (!strcmp(argv[1], "shared")) {
        bool enabled = false;
        if (argc != 4) {
            cli->sendMsg(ResponseCode::CommandSyntaxError,"Usage: volume shared <path> <method>", false);
            return 0;
        }

        if (vm->shareEnabled(argv[2], argv[3], &enabled)) {
            cli->sendMsg(ResponseCode::OperationFailed, "Failed to determine share enable state", true);
        } else {
            cli->sendMsg(ResponseCode::ShareEnabledResult,(enabled ? "Share enabled" : "Share disabled"), false);
        }
        return 0;
    } else if (!strcmp(argv[1], "mkdirs")) {
        if (argc != 3) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: volume mkdirs <path>", false);
            return 0;
        }
        rc = vm->mkdirs(argv[2]);
    } else {
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Unknown volume cmd", false);
    }

    if (!rc) {
        cli->sendMsg(ResponseCode::CommandOkay, "volume operation succeeded", false);
    } else {
        int erno = errno;
        rc = ResponseCode::convertFromErrno();
        cli->sendMsg(rc, "volume operation failed", true);
    }

    return 0;
}
