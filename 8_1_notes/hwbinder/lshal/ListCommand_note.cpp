//	@frameworks/native/cmds/lshal/ListCommand.cpp

ListCommand::ListCommand(Lshal &lshal) : mLshal(lshal), mErr(lshal.err()), mOut(lshal.out()) {
}


Status ListCommand::main(const std::string &command, const Arg &arg) {
    Status status = parseArgs(command, arg);
    if (status != OK) {
        return status;
    }
    status = fetch();
    postprocess();
    dump();
    return status;
}

Status ListCommand::parseArgs(const std::string &command, const Arg &arg) {
    static struct option longOptions[] = {
        // long options with short alternatives
        {"help",      no_argument,       0, 'h' },
        {"interface", no_argument,       0, 'i' },
        {"transport", no_argument,       0, 't' },
        {"arch",      no_argument,       0, 'r' },
        {"pid",       no_argument,       0, 'p' },
        {"address",   no_argument,       0, 'a' },
        {"clients",   no_argument,       0, 'c' },
        {"threads",   no_argument,       0, 'e' },
        {"cmdline",   no_argument,       0, 'm' },
        {"debug",     optional_argument, 0, 'd' },

        // long options without short alternatives
        {"sort",      required_argument, 0, 's' },
        {"init-vintf",optional_argument, 0, 'v' },
        {"neat",      no_argument,       0, 'n' },
        { 0,          0,                 0,  0  }
    };

    int optionIndex;
    int c;
    // Lshal::parseArgs has set optind to the next option to parse
    for (;;) {
        // using getopt_long in case we want to add other options in the future
        c = getopt_long(arg.argc, arg.argv,"hitrpacmde", longOptions, &optionIndex);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 's': {
            if (strcmp(optarg, "interface") == 0 || strcmp(optarg, "i") == 0) {
                mSortColumn = TableEntry::sortByInterfaceName;
            } else if (strcmp(optarg, "pid") == 0 || strcmp(optarg, "p") == 0) {
                mSortColumn = TableEntry::sortByServerPid;
            } else {
                mErr << "Unrecognized sorting column: " << optarg << std::endl;
                mLshal.usage(command);
                return USAGE;
            }
            break;
        }
        case 'v': {
            if (optarg) {
                mFileOutput = new std::ofstream{optarg};
                mOut = mFileOutput;
                if (!mFileOutput.buf().is_open()) {
                    mErr << "Could not open file '" << optarg << "'." << std::endl;
                    return IO_ERROR;
                }
            }
            mVintf = true;
        }
        case 'i': {
            mSelectedColumns |= ENABLE_INTERFACE_NAME;
            break;
        }
        case 't': {
            mSelectedColumns |= ENABLE_TRANSPORT;
            break;
        }
        case 'r': {
            mSelectedColumns |= ENABLE_ARCH;
            break;
        }
        case 'p': {
            mSelectedColumns |= ENABLE_SERVER_PID;
            break;
        }
        case 'a': {
            mSelectedColumns |= ENABLE_SERVER_ADDR;
            break;
        }
        case 'c': {
            mSelectedColumns |= ENABLE_CLIENT_PIDS;
            break;
        }
        case 'e': {
            mSelectedColumns |= ENABLE_THREADS;
            break;
        }
        case 'm': {
            mEnableCmdlines = true;
            break;
        }
        case 'd': {
            mEmitDebugInfo = true;

            if (optarg) {
                mFileOutput = new std::ofstream{optarg};
                mOut = mFileOutput;
                if (!mFileOutput.buf().is_open()) {
                    mErr << "Could not open file '" << optarg << "'." << std::endl;
                    return IO_ERROR;
                }
                chown(optarg, AID_SHELL, AID_SHELL);
            }
            break;
        }
        case 'n': {
            mNeat = true;
            break;
        }
        case 'h': // falls through
        default: // see unrecognized options
            mLshal.usage(command);
            return USAGE;
        }
    }
    if (optind < arg.argc) {
        // see non option
        mErr << "Unrecognized option `" << arg.argv[optind] << "`" << std::endl;
    }

    if (mSelectedColumns == 0) {
        mSelectedColumns = ENABLE_INTERFACE_NAME | ENABLE_SERVER_PID | ENABLE_CLIENT_PIDS | ENABLE_THREADS;
    }
    return OK;
}




Status ListCommand::fetch() {
    Status status = OK;
    auto bManager = mLshal.serviceManager();
    if (bManager == nullptr) {
        mErr << "Failed to get defaultServiceManager()!" << std::endl;
        status |= NO_BINDERIZED_MANAGER;
    } else {
        status |= fetchBinderized(bManager);
        // Passthrough PIDs are registered to the binderized manager as well.
        status |= fetchPassthrough(bManager);
    }

    auto pManager = mLshal.passthroughManager();
    if (pManager == nullptr) {
        mErr << "Failed to get getPassthroughServiceManager()!" << std::endl;
        status |= NO_PASSTHROUGH_MANAGER;
    } else {
        status |= fetchAllLibraries(pManager);
    }
    return status;
}

Status ListCommand::fetchBinderized(const sp<IServiceManager> &manager) {
    using namespace ::std;
    using namespace ::android::hardware;
    using namespace ::android::hidl::manager::V1_0;
    using namespace ::android::hidl::base::V1_0;
    const std::string mode = "hwbinder";

    hidl_vec<hidl_string> fqInstanceNames;
    // copying out for timeoutIPC
    auto listRet = timeoutIPC(manager, &IServiceManager::list, [&] (const auto &names) {
        fqInstanceNames = names;
    });
    if (!listRet.isOk()) {
        mErr << "Error: Failed to list services for " << mode << ": " << listRet.description() << std::endl;
        return DUMP_BINDERIZED_ERROR;
    }

    Status status = OK;
    // server pid, .ptr value of binder object, child pids
    std::map<std::string, DebugInfo> allDebugInfos;
    std::map<pid_t, PidInfo> allPids;
    for (const auto &fqInstanceName : fqInstanceNames) {
        const auto pair = splitFirst(fqInstanceName, '/');
        const auto &serviceName = pair.first;
        const auto &instanceName = pair.second;
        auto getRet = timeoutIPC(manager, &IServiceManager::get, serviceName, instanceName);
        if (!getRet.isOk()) {
            mErr << "Warning: Skipping \"" << fqInstanceName << "\": "  << "cannot be fetched from service manager:"   << getRet.description() << std::endl;
            status |= DUMP_BINDERIZED_ERROR;
            continue;
        }
        sp<IBase> service = getRet;
        if (service == nullptr) {
            mErr << "Warning: Skipping \"" << fqInstanceName << "\": "   << "cannot be fetched from service manager (null)" << std::endl;
            status |= DUMP_BINDERIZED_ERROR;
            continue;
        }
        auto debugRet = timeoutIPC(service, &IBase::getDebugInfo, [&] (const auto &debugInfo) {
            allDebugInfos[fqInstanceName] = debugInfo;
            if (debugInfo.pid >= 0) {
                allPids[static_cast<pid_t>(debugInfo.pid)] = PidInfo();
            }
        });
        if (!debugRet.isOk()) {
            mErr << "Warning: Skipping \"" << fqInstanceName << "\": "
                 << "debugging information cannot be retrieved:"
                 << debugRet.description() << std::endl;
            status |= DUMP_BINDERIZED_ERROR;
        }
    }

    for (auto &pair : allPids) {
        pid_t serverPid = pair.first;
        if (!getPidInfo(serverPid, &allPids[serverPid])) {
            mErr << "Warning: no information for PID " << serverPid << ", are you root?" << std::endl;
            status |= DUMP_BINDERIZED_ERROR;
        }
    }
    for (const auto &fqInstanceName : fqInstanceNames) {
        auto it = allDebugInfos.find(fqInstanceName);
        if (it == allDebugInfos.end()) {
            putEntry(HWSERVICEMANAGER_LIST, {
                .interfaceName = fqInstanceName,
                .transport = mode,
                .serverPid = NO_PID,
                .serverObjectAddress = NO_PTR,
                .clientPids = {},
                .threadUsage = 0,
                .threadCount = 0,
                .arch = ARCH_UNKNOWN
            });
            continue;
        }
        const DebugInfo &info = it->second;
        bool writePidInfo = info.pid != NO_PID && info.ptr != NO_PTR;

        putEntry(HWSERVICEMANAGER_LIST, {
            .interfaceName = fqInstanceName,
            .transport = mode,
            .serverPid = info.pid,
            .serverObjectAddress = info.ptr,
            .clientPids = writePidInfo ? allPids[info.pid].refPids[info.ptr] : Pids{},
            .threadUsage = writePidInfo ? allPids[info.pid].threadUsage : 0,
            .threadCount = writePidInfo ? allPids[info.pid].threadCount : 0,
            .arch = fromBaseArchitecture(info.arch),
        });
    }
    return status;
}


Status ListCommand::fetchPassthrough(const sp<IServiceManager> &manager) {
    using namespace ::android::hardware;
    using namespace ::android::hardware::details;
    using namespace ::android::hidl::manager::V1_0;
    using namespace ::android::hidl::base::V1_0;
    auto ret = timeoutIPC(manager, &IServiceManager::debugDump, [&] (const auto &infos) {
        for (const auto &info : infos) {
            if (info.clientPids.size() <= 0) {
                continue;
            }
            putEntry(PTSERVICEMANAGER_REG_CLIENT, {
                .interfaceName =  std::string{info.interfaceName.c_str()} + "/" + std::string{info.instanceName.c_str()},
                .transport = "passthrough",
                .serverPid = info.clientPids.size() == 1 ? info.clientPids[0] : NO_PID,
                .serverObjectAddress = NO_PTR,
                .clientPids = info.clientPids,
                .arch = fromBaseArchitecture(info.arch)
            });
        }
    });
    if (!ret.isOk()) {
        mErr << "Error: Failed to call debugDump on defaultServiceManager(): "   << ret.description() << std::endl;
        return DUMP_PASSTHROUGH_ERROR;
    }
    return OK;
}


template<class R, class P, class Function, class I, class... Args>
typename std::result_of<Function(I *, Args...)>::type
timeoutIPC(std::chrono::duration<R, P> wait, const sp<I> &interfaceObject, Function &&func,Args &&... args) {
    using ::android::hardware::Status;
    typename std::result_of<Function(I *, Args...)>::type ret{Status::ok()};
    auto boundFunc = std::bind(std::forward<Function>(func),interfaceObject.get(), std::forward<Args>(args)...);
    bool success = timeout(wait, [&ret, &boundFunc] {
        ret = std::move(boundFunc());
    });
    if (!success) {
        return Status::fromStatusT(TIMED_OUT);
    }
    return ret;
}




void ListCommand::postprocess() {
    forEachTable([this](Table &table) {
        if (mSortColumn) {
            std::sort(table.begin(), table.end(), mSortColumn);
        }
        for (TableEntry &entry : table) {
            entry.serverCmdline = getCmdline(entry.serverPid);
            removeDeadProcesses(&entry.clientPids);
            for (auto pid : entry.clientPids) {
                entry.clientCmdlines.push_back(this->getCmdline(pid));
            }
        }
    });
    // use a double for loop here because lshal doesn't care about efficiency.
    for (TableEntry &packageEntry : mImplementationsTable) {
        std::string packageName = packageEntry.interfaceName;
        FQName fqPackageName{packageName.substr(0, packageName.find("::"))};
        if (!fqPackageName.isValid()) {
            continue;
        }
        for (TableEntry &interfaceEntry : mPassthroughRefTable) {
            if (interfaceEntry.arch != ARCH_UNKNOWN) {
                continue;
            }
            FQName interfaceName{splitFirst(interfaceEntry.interfaceName, '/').first};
            if (!interfaceName.isValid()) {
                continue;
            }
            if (interfaceName.getPackageAndVersion() == fqPackageName) {
                interfaceEntry.arch = packageEntry.arch;
            }
        }
    }
}



void ListCommand::dump() {
    if (mVintf) {
        dumpVintf();
        if (!!mFileOutput) {
            mFileOutput.buf().close();
            delete &mFileOutput.buf();
            mFileOutput = nullptr;
        }
        mOut = std::cout;
    } else {
        dumpTable();
    }
}