//	@system/core/logcat/logcatd_main.cpp

int main(int argc, char** argv, char** envp) {
    android_logcat_context ctx = create_android_logcat();
    if (!ctx) return -1;

    signal(SIGPIPE, exit);

    // Save and detect presence of -L or --last flag
    std::vector<std::string> args;
    bool last = false;
    for (int i = 0; i < argc; ++i) {
        if (!argv[i]) continue;
        args.push_back(std::string(argv[i]));
        if (!strcmp(argv[i], "-L") || !strcmp(argv[i], "--last")) last = true;
    }

    // Generate argv from saved content
    std::vector<const char*> argv_hold;
    for (auto& str : args) argv_hold.push_back(str.c_str());
    argv_hold.push_back(nullptr);

    int ret = 0;
    if (last) {
        // Run logcat command with -L flag
        ret = android_logcat_run_command(ctx, -1, -1, argv_hold.size() - 1, (char* const*)&argv_hold[0], envp);
        // Remove -L and --last flags from argument list
        for (std::vector<const char*>::iterator it = argv_hold.begin();
             it != argv_hold.end();) {
            if (!*it || (strcmp(*it, "-L") && strcmp(*it, "--last"))) {
                ++it;
            } else {
                it = argv_hold.erase(it);
            }
        }
        // fall through to re-run the command regardless of the arguments
        // passed in.  For instance, we expect -h to report help stutter.
    }

    // Run logcat command without -L flag
    int retval = android_logcat_run_command(ctx, -1, -1, argv_hold.size() - 1, (char* const*)&argv_hold[0], envp);
    if (!ret) ret = retval;
    retval = android_logcat_destroy(&ctx);
    if (!ret) ret = retval;
    return ret;
}


