//	cl = new CommandListener();

CommandListener::CommandListener() : FrameworkListener("vold", true) {
    registerCmd(new DumpCmd()); // mCommand == "dump"
    registerCmd(new VolumeCmd());// mCommand == "volume"
    registerCmd(new AsecCmd());// mCommand == "asec"
    registerCmd(new ObbCmd());// mCommand == "obb"
    registerCmd(new StorageCmd());// mCommand == "storage"
    registerCmd(new XwarpCmd());// mCommand == "xwarp"
    registerCmd(new CryptfsCmd());// mCommand == "cryptfs"
    registerCmd(new FstrimCmd());// mCommand == "fstrim"
}


FrameworkListener::FrameworkListener(const char *socketName, bool withSeq) : SocketListener(socketName, true, withSeq) {
    init(socketName, withSeq);
}

void FrameworkListener::init(const char *socketName, bool withSeq) {
    //	typedef android::sysutils::List<FrameworkCommand *> FrameworkCommandCollection;  //FrameworkCommand system/core/libsysutils/src/FrameworkCommand.cpp
    mCommands = new FrameworkCommandCollection();
    errorRate = 0;
    mCommandCount = 0;
    mWithSeq = withSeq; //true
}


SocketListener::SocketListener(const char *socketName == "vold", bool listen == true) {
    init(socketName, -1, listen, false);
}

void SocketListener::init(const char *socketName, int socketFd = -1, bool listen, bool useCmdNum) {
    mListen = listen; //true
    mSocketName = socketName; //"vold"
    mSock = socketFd;	// -1
    mUseCmdNum = useCmdNum; // false
    pthread_mutex_init(&mClientsLock, NULL);
    mClients = new SocketClientCollection(); //	typedef android::sysutils::List<SocketClient *> SocketClientCollection;   //SocketClient	[system/core/libsysutils/src/SocketClient.cpp]
}

//	registerCmd(new DumpCmd());
void FrameworkListener::registerCmd(FrameworkCommand *cmd) {
    mCommands->push_back(cmd);
}

CommandListener::DumpCmd::DumpCmd() : VoldCommand("dump") {
}

VoldCommand::VoldCommand(const char *cmd) : FrameworkCommand(cmd)  {
}

FrameworkCommand::FrameworkCommand(const char *cmd) {
    mCommand = cmd;
}






























