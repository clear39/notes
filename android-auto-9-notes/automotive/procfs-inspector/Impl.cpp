std::vector<procfsinspector::ProcessInfo> procfsinspector::Impl::readProcessTable() {
    std::vector<procfsinspector::ProcessInfo> processes;

    Directory dir("/proc");
    while (auto entry = dir.next()) {
        pid_t pid;
        if (asNumber(entry.getChild(), &pid)) {
            processes.push_back(ProcessInfo{pid, entry.getOwnerUserId()});
        }
    }

    return processes;
}