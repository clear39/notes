//	@frameworks/native/cmds/lshal/main.cpp
int main(int argc, char **argv) {
    using namespace ::android::lshal;
    return Lshal{}.main(Arg{argc, argv});
}


Lshal::Lshal()
    : mOut(std::cout), mErr(std::cerr),
      mServiceManager(::android::hardware::defaultServiceManager()),
      mPassthroughManager(::android::hardware::getPassthroughServiceManager()) {
}


struct Arg {
    int argc;
    char **argv;
};


//	@frameworks/native/cmds/lshal/Lshal.cpp
Status Lshal::main(const Arg &arg) {
    // Allow SIGINT to terminate all threads.
    signal(SIGINT, signalHandler);

    Status status = parseArgs(arg);
    if (status != OK) {
        return status;
    }
    if (mCommand == "help") {
        usage(optind < arg.argc ? arg.argv[optind] : "");
        return USAGE;
    }
    // Default command is list
    if (mCommand == "list" || mCommand == "") {
        return ListCommand{*this}.main(mCommand, arg);
    }
    if (mCommand == "debug") {
        return DebugCommand{*this}.main(mCommand, arg);
    }
    usage();
    return USAGE;
}



void signalHandler(int sig) {
    if (sig == SIGINT) {
        int retVal;
        pthread_exit(&retVal);
    }
}


Status Lshal::parseArgs(const Arg &arg) {
    static std::set<std::string> sAllCommands{"list", "debug", "help"};
    optind = 1;//该变量由标准库定义 man getopt
    if (optind >= arg.argc) {
        // no options at all.
        return OK;
    }
    mCommand = arg.argv[optind];
    if (sAllCommands.find(mCommand) != sAllCommands.end()) {
        ++optind;//optind = 2
        return OK; // mCommand is set correctly
    }

    if (mCommand.size() > 0 && mCommand[0] == '-') {
        // first argument is an option, set command to "" (which is recognized as "list")
        mCommand = "";
        return OK;
    }

    mErr << arg.argv[0] << ": unrecognized option `" << arg.argv[optind] << "`" << std::endl;
    usage();
    return USAGE;
}
