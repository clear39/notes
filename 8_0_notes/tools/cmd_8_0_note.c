源码路径:	
//	frameworks/native/cmds/cmd/cmd.cpp
int main(int argc, char* const argv[])
{
    signal(SIGPIPE, SIG_IGN);
    sp<ProcessState> proc = ProcessState::self();
    // setThreadPoolMaxThreadCount(0) actually tells the kernel it's
    // not allowed to spawn any additional threads, but we still spawn
    // a binder thread from userspace when we call startThreadPool().
    // This is safe because we only have 2 callbacks, neither of which
    // block.
    // See b/36066697 for rationale
    proc->setThreadPoolMaxThreadCount(0);
    proc->startThreadPool();

    sp<IServiceManager> sm = defaultServiceManager();
    fflush(stdout);
    if (sm == NULL) {
        ALOGW("Unable to get default service manager!");
        aerr << "cmd: Unable to get default service manager!" << endl;
        return 20;
    }

    if (argc == 1) {
        aerr << "cmd: No service specified; use -l to list all services" << endl;
        return 20;
    }

    if ((argc == 2) && (strcmp(argv[1], "-l") == 0)) {//cmd -l //查看
        Vector<String16> services = sm->listServices();
        services.sort(sort_func);
        aout << "Currently running services:" << endl;

        for (size_t i=0; i<services.size(); i++) {
            sp<IBinder> service = sm->checkService(services[i]);
            if (service != NULL) {
                aout << "  " << services[i] << endl;
            }
        }
        return 0;
    }

    Vector<String16> args;
    for (int i=2; i<argc; i++) {
        args.add(String16(argv[i]));
    }
    String16 cmd = String16(argv[1]);
    sp<IBinder> service = sm->checkService(cmd);
    if (service == NULL) {
        ALOGW("Can't find service %s", argv[1]);
        aerr << "cmd: Can't find service: " << argv[1] << endl;
        return 20;
    }

    sp<MyShellCallback> cb = new MyShellCallback();
    sp<MyResultReceiver> result = new MyResultReceiver();

#if DEBUG
    ALOGD("cmd: Invoking %s in=%d, out=%d, err=%d", argv[1], STDIN_FILENO, STDOUT_FILENO,STDERR_FILENO);
#endif

    // TODO: block until a result is returned to MyResultReceiver.
    status_t err = IBinder::shellCommand(service, STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO, args,cb, result);
    if (err < 0) {
        const char* errstr;
        switch (err) {
            case BAD_TYPE: errstr = "Bad type"; break;
            case FAILED_TRANSACTION: errstr = "Failed transaction"; break;
            case FDS_NOT_ALLOWED: errstr = "File descriptors not allowed"; break;
            case UNEXPECTED_NULL: errstr = "Unexpected null"; break;
            default: errstr = strerror(-err); break;
        }
        ALOGW("Failure calling service %s: %s (%d)", argv[1], errstr, -err);
        aout << "cmd: Failure calling service " << argv[1] << ": " << errstr << " (" << (-err) << ")" << endl;
        return err;
    }

    cb->mActive = false;
    status_t res = result->waitForResult();
#if DEBUG
    ALOGD("result=%d", (int)res);
#endif
    return res;
}



//	frameworks/native/libs/binder/Binder.cpp
status_t IBinder::shellCommand(const sp<IBinder>& target, int in, int out, int err,
    Vector<String16>& args, const sp<IShellCallback>& callback,const sp<IResultReceiver>& resultReceiver)
{
    Parcel send;
    Parcel reply;
    send.writeFileDescriptor(in);
    send.writeFileDescriptor(out);
    send.writeFileDescriptor(err);
    const size_t numArgs = args.size();
    send.writeInt32(numArgs);
    for (size_t i = 0; i < numArgs; i++) {
        send.writeString16(args[i]);
    }
    send.writeStrongBinder(callback != NULL ? IInterface::asBinder(callback) : NULL);
    send.writeStrongBinder(resultReceiver != NULL ? IInterface::asBinder(resultReceiver) : NULL);
    return target->transact(SHELL_COMMAND_TRANSACTION, send, &reply);
}



