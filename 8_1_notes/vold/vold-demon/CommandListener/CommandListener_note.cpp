//	@system/vold/CommandListener.cpp
CommandListener::VolumeCmd::VolumeCmd() :
                 VoldCommand("volume") {
}

int CommandListener::VolumeCmd::runCommand(SocketClient *cli,
                                           int argc, char **argv) {
    dumpArgs(argc, argv, -1);

    if (argc < 2) {
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Missing Argument", false);
        return 0;
    }

    VolumeManager *vm = VolumeManager::Instance();
    std::lock_guard<std::mutex> lock(vm->getLock());

    // TODO: tease out methods not directly related to volumes

    std::string cmd(argv[1]);
    if (cmd == "reset") {
        return sendGenericOkFail(cli, vm->reset());

    } else if (cmd == "shutdown") {
        return sendGenericOkFail(cli, vm->shutdown());

    } else if (cmd == "debug") {
        return sendGenericOkFail(cli, vm->setDebug(true));

    } else if (cmd == "partition" && argc > 3) {
        // partition [diskId] [public|private|mixed] [ratio]
        std::string id(argv[2]);
        auto disk = vm->findDisk(id);
        if (disk == nullptr) {
            return cli->sendMsg(ResponseCode::CommandSyntaxError, "Unknown disk", false);
        }

        std::string type(argv[3]);
        if (type == "public") {
            return sendGenericOkFail(cli, disk->partitionPublic());
        } else if (type == "private") {
            return sendGenericOkFail(cli, disk->partitionPrivate());
        } else if (type == "mixed") {
            if (argc < 4) {
                return cli->sendMsg(ResponseCode::CommandSyntaxError, nullptr, false);
            }
            int frac = atoi(argv[4]);
            return sendGenericOkFail(cli, disk->partitionMixed(frac));
        } else {
            return cli->sendMsg(ResponseCode::CommandSyntaxError, nullptr, false);
        }

    } else if (cmd == "mkdirs" && argc > 2) {
        // mkdirs [path]
        return sendGenericOkFail(cli, vm->mkdirs(argv[2]));

    } else if (cmd == "user_added" && argc > 3) {
        // user_added [user] [serial]
        return sendGenericOkFail(cli, vm->onUserAdded(atoi(argv[2]), atoi(argv[3])));

    } else if (cmd == "user_removed" && argc > 2) {
        // user_removed [user]
        return sendGenericOkFail(cli, vm->onUserRemoved(atoi(argv[2])));

    } else if (cmd == "user_started" && argc > 2) {
        // user_started [user]
        return sendGenericOkFail(cli, vm->onUserStarted(atoi(argv[2])));

    } else if (cmd == "user_stopped" && argc > 2) {
        // user_stopped [user]
        return sendGenericOkFail(cli, vm->onUserStopped(atoi(argv[2])));

    } else if (cmd == "mount" && argc > 2) {//接收到 StorageManagerService 发送过来的 D VoldConnector: SND -> {31 volume mount public:8,1 2 0}
        // mount [volId] [flags] [user]
        std::string id(argv[2]);
        auto vol = vm->findVolume(id);
        if (vol == nullptr) {
            return cli->sendMsg(ResponseCode::CommandSyntaxError, "Unknown volume", false);
        }

        int mountFlags = (argc > 3) ? atoi(argv[3]) : 0;
        userid_t mountUserId = (argc > 4) ? atoi(argv[4]) : -1;

        vol->setMountFlags(mountFlags);
        vol->setMountUserId(mountUserId);

        int res = vol->mount();
        if (mountFlags & android::vold::VolumeBase::MountFlags::kPrimary) {
            vm->setPrimary(vol);
        }
        return sendGenericOkFail(cli, res);

    } else if (cmd == "unmount" && argc > 2) {
        // unmount [volId]
        std::string id(argv[2]);
        auto vol = vm->findVolume(id);
        if (vol == nullptr) {
            return cli->sendMsg(ResponseCode::CommandSyntaxError, "Unknown volume", false);
        }

        return sendGenericOkFail(cli, vol->unmount());

    } else if (cmd == "format" && argc > 3) {
        // format [volId] [fsType|auto]
        std::string id(argv[2]);
        std::string fsType(argv[3]);
        auto vol = vm->findVolume(id);
        if (vol == nullptr) {
            return cli->sendMsg(ResponseCode::CommandSyntaxError, "Unknown volume", false);
        }

        return sendGenericOkFail(cli, vol->format(fsType));

    } else if (cmd == "move_storage" && argc > 3) {
        // move_storage [fromVolId] [toVolId]
        auto fromVol = vm->findVolume(std::string(argv[2]));
        auto toVol = vm->findVolume(std::string(argv[3]));
        if (fromVol == nullptr || toVol == nullptr) {
            return cli->sendMsg(ResponseCode::CommandSyntaxError, "Unknown volume", false);
        }

        (new android::vold::MoveTask(fromVol, toVol))->start();
        return sendGenericOkFail(cli, 0);

    } else if (cmd == "benchmark" && argc > 2) {
        // benchmark [volId]
        std::string id(argv[2]);
        nsecs_t res = vm->benchmarkPrivate(id);
        return cli->sendMsg(ResponseCode::CommandOkay,
                android::base::StringPrintf("%" PRId64, res).c_str(), false);

    } else if (cmd == "forget_partition" && argc > 2) {
        // forget_partition [partGuid]
        std::string partGuid(argv[2]);
        return sendGenericOkFail(cli, vm->forgetPartition(partGuid));

    } else if (cmd == "remount_uid" && argc > 3) {
        // remount_uid [uid] [none|default|read|write]
        uid_t uid = atoi(argv[2]);
        std::string mode(argv[3]);
        return sendGenericOkFail(cli, vm->remountUid(uid, mode));
    }

    return cli->sendMsg(ResponseCode::CommandSyntaxError, nullptr, false);
}