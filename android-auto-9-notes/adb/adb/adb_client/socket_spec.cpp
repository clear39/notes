
//  @   system/core/adb/socket_spec.cpp
#define ADB_HOST 1

struct LocalSocketType {
    int socket_namespace;
    bool available;
};

static auto& kLocalSocketTypes = *new std::unordered_map<std::string, LocalSocketType>({
#if ADB_HOST
    { "local", { ANDROID_SOCKET_NAMESPACE_FILESYSTEM, !ADB_WINDOWS } },
#else
    { "local", { ANDROID_SOCKET_NAMESPACE_RESERVED, !ADB_WINDOWS } },
#endif

    { "localreserved", { ANDROID_SOCKET_NAMESPACE_RESERVED, !ADB_HOST } },
    { "localabstract", { ANDROID_SOCKET_NAMESPACE_ABSTRACT, ADB_LINUX } },
    { "localfilesystem", { ANDROID_SOCKET_NAMESPACE_FILESYSTEM, !ADB_WINDOWS } },
});



int socket_spec_connect(const std::string& spec, std::string* error) {
    if (StartsWith(spec, "tcp:")) {
        std::string hostname;
        int port;
        if (!parse_tcp_socket_spec(spec, &hostname, &port, error)) {
            return -1;
        }

        int result;
        if (tcp_host_is_local(hostname)) {
            result = network_loopback_client(port, SOCK_STREAM, error);
        } else {
#if ADB_HOST
            result = network_connect(hostname, port, SOCK_STREAM, 0, error);
#else
            // Disallow arbitrary connections in adbd.
            *error = "adbd does not support arbitrary tcp connections";
            return -1;
#endif
        }

        if (result >= 0) {
            disable_tcp_nagle(result);
        }
        return result;
    }

    for (const auto& it : kLocalSocketTypes) {
        std::string prefix = it.first + ":";
        if (StartsWith(spec, prefix)) {
            if (!it.second.available) {
                *error = StringPrintf("socket type %s is unavailable on this platform",it.first.c_str());
                return -1;
            }

            return network_local_client(&spec[prefix.length()], it.second.socket_namespace,
                                        SOCK_STREAM, error);
        }
    }

    *error = StringPrintf("unknown socket specification '%s'", spec.c_str());
    return -1;
}


bool parse_tcp_socket_spec(const std::string& spec, std::string* hostname, int* port,std::string* error) {
    if (!StartsWith(spec, "tcp:")) {
        *error = StringPrintf("specification is not tcp: '%s'", spec.c_str());
        return false;
    }

    std::string hostname_value;
    int port_value;

    // If the spec is tcp:<port>, parse it ourselves.
    // Otherwise, delegate to android::base::ParseNetAddress.
    if (android::base::ParseInt(&spec[4], &port_value)) {
        // Do the range checking ourselves, because ParseInt rejects 'tcp:65536' and 'tcp:foo:1234'
        // identically.
        if (port_value < 0 || port_value > 65535) {
            *error = StringPrintf("bad port number '%d'", port_value);
            return false;
        }
    } else {
        std::string addr = spec.substr(4);
        port_value = -1;

        // FIXME: ParseNetAddress rejects port 0. This currently doesn't hurt, because listening
        //        on an address that isn't 'localhost' is unsupported.
        if (!android::base::ParseNetAddress(addr, &hostname_value, &port_value, nullptr, error)) {
            return false;
        }

        if (port_value == -1) {
            *error = StringPrintf("missing port in specification: '%s'", spec.c_str());
            return false;
        }
    }

    if (hostname) {
        *hostname = std::move(hostname_value);
    }

    if (port) {
        *port = port_value;
    }

    return true;
}

static bool tcp_host_is_local(const std::string& hostname) {
    // FIXME
    return hostname.empty() || hostname == "localhost";
}
