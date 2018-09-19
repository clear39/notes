int CommandListener::StorageCmd::runCommand(SocketClient *cli,int argc, char **argv) {
    dumpArgs(argc, argv, -1);

    if (argc < 2) {
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Missing Argument", false);
        return 0;
    }

    if (!strcmp(argv[1], "users")) {
        DIR *dir;
        struct dirent *de;

        if (!(dir = opendir("/proc"))) {
            cli->sendMsg(ResponseCode::OperationFailed, "Failed to open /proc", true);
            return 0;
        }

        while ((de = readdir(dir))) {
            int pid = Process::getPid(de->d_name);

            if (pid < 0) {
                continue;
            }

            char processName[255];
            Process::getProcessName(pid, processName, sizeof(processName));

            if (Process::checkFileDescriptorSymLinks(pid, argv[2]) ||
                Process::checkFileMaps(pid, argv[2]) ||
                Process::checkSymLink(pid, argv[2], "cwd") ||
                Process::checkSymLink(pid, argv[2], "root") ||
                Process::checkSymLink(pid, argv[2], "exe")) {

                char msg[1024];
                snprintf(msg, sizeof(msg), "%d %s", pid, processName);
                cli->sendMsg(ResponseCode::StorageUsersListResult, msg, false);
            }
        }
        closedir(dir);
        cli->sendMsg(ResponseCode::CommandOkay, "Storage user list complete", false);
    } else {
        cli->sendMsg(ResponseCode::CommandSyntaxError, "Unknown storage cmd", false);
    }
    return 0;
}
