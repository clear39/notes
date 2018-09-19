int CommandListener::FstrimCmd::runCommand(SocketClient *cli,int argc, char **argv) {
    if ((cli->getUid() != 0) && (cli->getUid() != AID_SYSTEM)) {
        cli->sendMsg(ResponseCode::CommandNoPermission, "No permission to run fstrim commands", false);
        return 0;
    }

    if (argc < 2) {
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Missing Argument", false);
        return 0;
    }

    int rc = 0;

    if (!strcmp(argv[1], "dotrim")) {
        if (argc != 2) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: fstrim dotrim", false);
            return 0;
        }
        dumpArgs(argc, argv, -1);
        rc = fstrim_filesystems();
    } else {
        dumpArgs(argc, argv, -1);
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Unknown fstrim cmd", false);
    }

    // Always report that the command succeeded and return the error code.
    // The caller will check the return value to see what the error was.
    char msg[255];
    snprintf(msg, sizeof(msg), "%d", rc);
    cli->sendMsg(ResponseCode::CommandOkay, msg, false);

    return 0;
}
