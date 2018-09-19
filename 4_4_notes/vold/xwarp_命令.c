int CommandListener::XwarpCmd::runCommand(SocketClient *cli,int argc, char **argv) {
    if (argc < 2) {
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Missing Argument", false);
        return 0;
    }

    if (!strcmp(argv[1], "enable")) {
        if (Xwarp::enable()) {
            cli->sendMsg(ResponseCode::OperationFailed, "Failed to enable xwarp", true);
            return 0;
        }

        cli->sendMsg(ResponseCode::CommandOkay, "Xwarp mirroring started", false);
    } else if (!strcmp(argv[1], "disable")) {
        if (Xwarp::disable()) {
            cli->sendMsg(ResponseCode::OperationFailed, "Failed to disable xwarp", true);
            return 0;
        }

        cli->sendMsg(ResponseCode::CommandOkay, "Xwarp disabled", false);
    } else if (!strcmp(argv[1], "status")) {
        char msg[255];
        bool r; 	
        unsigned mirrorPos, maxSize;

        if (Xwarp::status(&r, &mirrorPos, &maxSize)) {
            cli->sendMsg(ResponseCode::OperationFailed, "Failed to get xwarp status", true);
            return 0;
        }
        snprintf(msg, sizeof(msg), "%s %u %u", (r ? "ready" : "not-ready"), mirrorPos, maxSize);
        cli->sendMsg(ResponseCode::XwarpStatusResult, msg, false);
    } else {
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Unknown storage cmd", false);
    }

    return 0;
}
