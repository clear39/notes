

//  @  system/core/adb/client/main.cpp 
int main(int argc, char** argv) {
    adb_trace_init(argv);
    return adb_commandline(argc - 1, const_cast<const char**>(argv + 1));
}


