int CommandListener::CryptfsCmd::runCommand(SocketClient *cli,int argc, char **argv) {
    if ((cli->getUid() != 0) && (cli->getUid() != AID_SYSTEM)) {
        cli->sendMsg(ResponseCode::CommandNoPermission, "No permission to run cryptfs commands", false);
        return 0;
    }

    if (argc < 2) {
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Missing Argument", false);
        return 0;
    }

    int rc = 0;

    if (!strcmp(argv[1], "checkpw")) {
        if (argc != 3) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: cryptfs checkpw <passwd>", false);
            return 0;
        }
        dumpArgs(argc, argv, 2);
        rc = cryptfs_check_passwd(argv[2]);
    } else if (!strcmp(argv[1], "restart")) {
        if (argc != 2) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: cryptfs restart", false);
            return 0;
        }
        dumpArgs(argc, argv, -1);
        rc = cryptfs_restart();
    } else if (!strcmp(argv[1], "cryptocomplete")) {
        if (argc != 2) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: cryptfs cryptocomplete", false);
            return 0;
        }
        dumpArgs(argc, argv, -1);
        rc = cryptfs_crypto_complete();
    } else if (!strcmp(argv[1], "enablecrypto")) {
        if ( (argc != 4) || (strcmp(argv[2], "wipe") && strcmp(argv[2], "inplace")) ) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: cryptfs enablecrypto <wipe|inplace> <passwd>", false);
            return 0;
        }
        dumpArgs(argc, argv, 3);
        rc = cryptfs_enable(argv[2], argv[3]);
    } else if (!strcmp(argv[1], "changepw")) {
        if (argc != 3) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: cryptfs changepw <newpasswd>", false);
            return 0;
        } 
        SLOGD("cryptfs changepw {}");
        rc = cryptfs_changepw(argv[2]);
    } else if (!strcmp(argv[1], "verifypw")) {
        if (argc != 3) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: cryptfs verifypw <passwd>", false);
            return 0;
        }
        SLOGD("cryptfs verifypw {}");
        rc = cryptfs_verify_passwd(argv[2]);
    } else if (!strcmp(argv[1], "getfield")) {
        char valbuf[PROPERTY_VALUE_MAX];

        if (argc != 3) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: cryptfs getfield <fieldname>", false);
            return 0;
        }
        dumpArgs(argc, argv, -1);
        rc = cryptfs_getfield(argv[2], valbuf, sizeof(valbuf));
        if (rc == 0) {
            cli->sendMsg(ResponseCode::CryptfsGetfieldResult, valbuf, false);
        }
    } else if (!strcmp(argv[1], "setfield")) {
        if (argc != 4) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, "Usage: cryptfs setfield <fieldname> <value>", false);
            return 0;
        }
        dumpArgs(argc, argv, -1);
        rc = cryptfs_setfield(argv[2], argv[3]);
    } else {
        dumpArgs(argc, argv, -1);
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Unknown cryptfs cmd", false);
    }

    // Always report that the command succeeded and return the error code.
    // The caller will check the return value to see what the error was.
    char msg[255];
    snprintf(msg, sizeof(msg), "%d", rc);
    cli->sendMsg(ResponseCode::CommandOkay, msg, false);

    return 0;
}
