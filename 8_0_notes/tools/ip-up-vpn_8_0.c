


/*
 * The primary goal is to create a file with VPN parameters. Currently they
 * are interface, addresses, routes, DNS servers, and search domains and VPN
 * server address. Each parameter occupies(占有) one line in the file, and it can be
 * an empty string or space-separated values. The order and the format must be
 * consistent with com.android.server.connectivity.Vpn. Here is an example.
 *
 *   ppp0
 *   192.168.1.100/24
 *   0.0.0.0/0
 *   192.168.1.1 192.168.1.2
 *   example.org
 *   192.0.2.1
 *
 * The secondary goal is to unify(统一) the outcome(结果;成果;出路) of VPN. The current baseline
 * is to have an interface configured with the given address and netmask
 * and maybe add a host route to protect the tunnel. PPP-based VPN already
 * does this, but others might not. Routes, DNS servers, and search domains
 * are handled by the framework since they can be overridden by the users.
 */

int main(int argc, char **argv)
{
    FILE *state = fopen(DIR ".tmp", "wb"); //	#define DIR "/data/misc/vpn/"
    if (!state) {
        ALOGE("Cannot create state: %s", strerror(errno));
        return 1;
    }

    if (argc >= 6) {
        /* Invoked by pppd. */
        fprintf(state, "%s\n", argv[1]);
        fprintf(state, "%s/32\n", argv[4]);
        fprintf(state, "0.0.0.0/0\n");
        fprintf(state, "%s %s\n", env("DNS1"), env("DNS2")); //
        fprintf(state, "\n");
        fprintf(state, "\n");
    } else if (argc == 2) {
        /* Invoked by racoon. */
        const char *interface = env("INTERFACE");
        const char *address = env("INTERNAL_ADDR4");
        const char *routes = env("SPLIT_INCLUDE_CIDR");

        int s = socket(AF_INET, SOCK_DGRAM, 0);
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));

        /* Bring up the interface. */
        ifr.ifr_flags = IFF_UP;
        strncpy(ifr.ifr_name, interface, IFNAMSIZ);
        if (ioctl(s, SIOCSIFFLAGS, &ifr)) {
            ALOGE("Cannot bring up %s: %s", interface, strerror(errno));
            return 1;
        }

        /* Set the address. */
        if (!set_address(&ifr.ifr_addr, address) || ioctl(s, SIOCSIFADDR, &ifr)) {
            ALOGE("Cannot set address: %s", strerror(errno));
            return 1;
        }

        /* Set the netmask. */
        if (set_address(&ifr.ifr_netmask, env("INTERNAL_NETMASK4"))) {
            if (ioctl(s, SIOCSIFNETMASK, &ifr)) {
                ALOGE("Cannot set netmask: %s", strerror(errno));
                return 1;
            }
        }

        /* TODO: Send few packets to trigger phase 2? */

        fprintf(state, "%s\n", interface);
        fprintf(state, "%s/%s\n", address, env("INTERNAL_CIDR4"));
        fprintf(state, "%s\n", routes[0] ? routes : "0.0.0.0/0");
        fprintf(state, "%s\n", env("INTERNAL_DNS4_LIST"));
        fprintf(state, "%s\n", env("DEFAULT_DOMAIN"));
        fprintf(state, "%s\n", env("REMOTE_ADDR"));
    } else {
        ALOGE("Cannot parse parameters");
        return 1;
    }

    fclose(state);
    if (chmod(DIR ".tmp", 0444) || rename(DIR ".tmp", DIR "state")) {
        ALOGE("Cannot write state: %s", strerror(errno));
        return 1;
    }
    return 0;
}

//	获取环境变量
static const char *env(const char *name) {
    const char *value = getenv(name);
    return value ? value : "";
}

