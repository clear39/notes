//  @   system/core/adb/transport.cpp
/**
 * int adbd_main(int server_port) 
 * --> 
 * */
void init_transport_registration(void) {
    int s[2];
    /**
     * system/core/adb/sysdeps.h:589 
     * adb_socketpair 调用 rc = unix_socketpair( AF_UNIX, SOCK_STREAM, 0, sv );
     * unix_socketpair 就是对 socketpair 封装
     * 
    */
    if (adb_socketpair(s)) {
        fatal_errno("cannot open transport registration socketpair");
    }
    D("socketpair: (%d,%d)", s[0], s[1]);

    transport_registration_send = s[0];
    transport_registration_recv = s[1];

    fdevent_install(&transport_registration_fde, transport_registration_recv,transport_registration_func, 0);

    fdevent_set(&transport_registration_fde, FDE_READ);
}




static void transport_registration_func(int _fd, unsigned ev, void* data) {
    tmsg m;
    int s[2];
    atransport* t;

    if (!(ev & FDE_READ)) {
        return;
    }

    if (transport_read_action(_fd, &m)) {
        fatal_errno("cannot read transport registration socket");
    }

    t = m.transport;

    if (m.action == 0) {
        D("transport: %s removing and free'ing %d", t->serial, t->transport_socket);

        /* IMPORTANT: the remove closes one half of the
        ** socket pair.  The close closes the other half.
        */
        fdevent_remove(&(t->transport_fde));
        adb_close(t->fd);

        {
            std::lock_guard<std::recursive_mutex> lock(transport_lock);
            transport_list.remove(t);
        }

        if (t->product) free(t->product);
        if (t->serial) free(t->serial);
        if (t->model) free(t->model);
        if (t->device) free(t->device);
        if (t->devpath) free(t->devpath);

        delete t;

        update_transports();
        return;
    }

    /* don't create transport threads for inaccessible devices */
    /**
     * usb 的 atransport kCsOffline
    */
    if (t->GetConnectionState() != kCsNoPerm) {
        /* initial references are the two threads */
        t->ref_count = 2;

        if (adb_socketpair(s)) {
            fatal_errno("cannot open transport socketpair");
        }

        D("transport: %s socketpair: (%d,%d) starting", t->serial, s[0], s[1]);

        t->transport_socket = s[0];
        t->fd = s[1];

        /**
         * 这里进行时间监听接口注册
        */
        fdevent_install(&(t->transport_fde), t->transport_socket, transport_socket_events, t);

        fdevent_set(&(t->transport_fde), FDE_READ);
        /**
         * 创建俩个线程
        */
        std::thread(write_transport_thread, t).detach();
        std::thread(read_transport_thread, t).detach();
    }

    {
        std::lock_guard<std::recursive_mutex> lock(transport_lock);
        pending_list.remove(t);
        transport_list.push_front(t);
    }

    /**
     * 由于这里分析的是adbd，所以update_transports 没有任何实现
    */
    update_transports();
}

// write_transport thread gets packets sent by the main thread (through send_packet()),
// and writes to a transport (representing a usb/tcp connection).
static void write_transport_thread(void* _t) {
    atransport* t = reinterpret_cast<atransport*>(_t);
    apacket* p;
    int active = 0;

    adb_thread_setname(android::base::StringPrintf("->%s", (t->serial != nullptr ? t->serial : "transport")));
    D("%s: starting write_transport thread, reading from fd %d", t->serial, t->fd);

    for (;;) {
        ATRACE_NAME("write_transport loop");
        /**
         * t->fd 是在 transport_registration_func 创建的socket对一端
         * 这里阻塞等待 read_transport_thread 写入 
        */
        if (read_packet(t->fd, t->serial, &p)) {
            D("%s: failed to read apacket from transport on fd %d", t->serial, t->fd);
            break;
        }

        if (p->msg.command == A_SYNC) {
            if (p->msg.arg0 == 0) {
                D("%s: transport SYNC offline", t->serial);
                /**
                 * put_apacket 内部实现 delete p;
                */
                put_apacket(p);
                break;
            } else {
                /**
                 * p->msg.arg1 是由 t->sync_token 在 read_transport_thread 累加赋值
                */
                if (p->msg.arg1 == t->sync_token) {
                    //  adbd D 11-09 08:53:48  2766  2768 transport.cpp:337] (null): transport SYNC online
                    D("%s: transport SYNC online", t->serial);
                    active = 1;
                } else {
                    D("%s: transport ignoring SYNC %d != %d", t->serial, p->msg.arg1, t->sync_token);
                }
            }
        } else {
            if (active) {
                D("%s: transport got packet, sending to remote", t->serial);
                ATRACE_NAME("write_transport write_remote");

                // Allow sending the payload's implicit null terminator.
                if (p->msg.data_length != p->payload.size()) {
                    LOG(FATAL) << "packet data length doesn't match payload: msg.data_length = " << p->msg.data_length << ", payload.size() = " << p->payload.size();
                }
                /*
                int atransport::Write(apacket* p) {
                    return this->connection->Write(p) ? 0 : -1;
                }
                */
                if (t->Write(p) != 0) {
                    D("%s: remote write failed for transport", t->serial);
                    put_apacket(p);
                    break;
                }
            } else {
                D("%s: transport ignoring packet while offline", t->serial);
            }
        }
        /**
         * put_apacket 内部实现 delete p;
        */
        put_apacket(p);
    }

    D("%s: write_transport thread is exiting, fd %d", t->serial, t->fd);
    kick_transport(t);
    transport_unref(t);
}


// The transport is opened by transport_register_func before
// the read_transport and write_transport threads are started.
//
// The read_transport thread issues a SYNC(1, token) message to let
// the write_transport thread know to start things up.  In the event
// of transport IO failure, the read_transport thread will post a
// SYNC(0,0) message to ensure shutdown.
//
// The transport will not actually be closed until both threads exit, but the threads
// will kick the transport on their way out to disconnect the underlying device.
//
// read_transport thread reads data from a transport (representing a usb/tcp connection),
// and makes the main thread call handle_packet().
static void read_transport_thread(void* _t) {
    atransport* t = reinterpret_cast<atransport*>(_t);
    apacket* p;

    adb_thread_setname(android::base::StringPrintf("<-%s", (t->serial != nullptr ? t->serial : "transport")));
    D("%s: starting read_transport thread on fd %d, SYNC online (%d)", t->serial, t->fd,t->sync_token + 1);
    p = get_apacket();
    p->msg.command = A_SYNC;
    p->msg.arg0 = 1;
    p->msg.arg1 = ++(t->sync_token);
    p->msg.magic = A_SYNC ^ 0xffffffff;
    D("sending SYNC packet (len = %u, payload.size() = %zu)", p->msg.data_length, p->payload.size());
    /**
     * 
    */
    if (write_packet(t->fd, t->serial, &p)) {
        put_apacket(p);
        D("%s: failed to write SYNC packet", t->serial);
        goto oops;
    }

    D("%s: data pump started", t->serial);
    for (;;) {
        ATRACE_NAME("read_transport loop");
        /**
         * new apacket ，然后 清 0
        */
        p = get_apacket();

        {
            ATRACE_NAME("read_transport read_remote");
            /**
             * 是在 void init_usb_transport(atransport* t, usb_handle* h) 创建
            */
            if (!t->connection->Read(p)) {
                D("%s: remote read failed for transport", t->serial);
                put_apacket(p);
                break;
            }

            /**
             * 
            */
            if (!check_header(p, t)) {
                D("%s: remote read: bad header", t->serial);
                put_apacket(p);
                break;
            }

#if ADB_HOST
            if (p->msg.command == 0) {
                put_apacket(p);
                continue;
            }
#endif
        }

        D("%s: received remote packet, sending to transport", t->serial);
        if (write_packet(t->fd, t->serial, &p)) {
            put_apacket(p);
            D("%s: failed to write apacket to transport", t->serial);
            goto oops;
        }
    }

    D("%s: SYNC offline for transport", t->serial);
    p = get_apacket();
    p->msg.command = A_SYNC;
    p->msg.arg0 = 0;
    p->msg.arg1 = 0;
    p->msg.magic = A_SYNC ^ 0xffffffff;
    if (write_packet(t->fd, t->serial, &p)) {
        put_apacket(p);
        D("%s: failed to write SYNC apacket to transport", t->serial);
    }

oops:
    D("%s: read_transport thread is exiting", t->serial);
    kick_transport(t);
    transport_unref(t);
}


static void transport_socket_events(int fd, unsigned events, void* _t) {
    atransport* t = reinterpret_cast<atransport*>(_t);
    D("transport_socket_events(fd=%d, events=%04x,...)", fd, events);
    if (events & FDE_READ) {
        apacket* p = 0;
        /**
         * fd 为 t->transport_socket 
        */
        if (read_packet(fd, t->serial, &p)) {
            D("%s: failed to read packet from transport socket on fd %d", t->serial, fd);
            return;
        }
        /**
         * system/core/adb/adb.cpp:342:
         * void handle_packet(apacket *p, atransport *t)
        */
        handle_packet(p, (atransport*)_t);
    }
}


void handle_packet(apacket *p, atransport *t)
{
    D("handle_packet() %c%c%c%c", ((char*) (&(p->msg.command)))[0],
            ((char*) (&(p->msg.command)))[1],
            ((char*) (&(p->msg.command)))[2],
            ((char*) (&(p->msg.command)))[3]);
    print_packet("recv", p);
    CHECK_EQ(p->payload.size(), p->msg.data_length);

    switch(p->msg.command){
    case A_SYNC:
        if (p->msg.arg0){
            send_packet(p, t);
#if ADB_HOST
            send_connect(t);
#endif
        } else {
            t->SetConnectionState(kCsOffline);
            handle_offline(t);
            send_packet(p, t);
        }
        return;

    case A_CNXN:  // CONNECT(version, maxdata, "system-id-string")
        handle_new_connection(t, p);
        break;

    case A_AUTH:
        switch (p->msg.arg0) {
#if ADB_HOST
            case ADB_AUTH_TOKEN:
                if (t->GetConnectionState() == kCsOffline) {
                    t->SetConnectionState(kCsUnauthorized);
                }
                send_auth_response(p->payload.data(), p->msg.data_length, t);
                break;
#else
            case ADB_AUTH_SIGNATURE:
                if (adbd_auth_verify(t->token, sizeof(t->token), p->payload)) {
                    adbd_auth_verified(t);
                    t->failed_auth_attempts = 0;
                } else {
                    if (t->failed_auth_attempts++ > 256) std::this_thread::sleep_for(1s);
                    send_auth_request(t);
                }
                break;

            case ADB_AUTH_RSAPUBLICKEY:
                adbd_auth_confirm_key(p->payload.data(), p->msg.data_length, t);
                break;
#endif
            default:
                t->SetConnectionState(kCsOffline);
                handle_offline(t);
                break;
        }
        break;

    case A_OPEN: /* OPEN(local-id, 0, "destination") */
        if (t->online && p->msg.arg0 != 0 && p->msg.arg1 == 0) {
            /**
             * @    system/core/adb/sockets.cpp
            */
            asocket* s = create_local_service_socket(p->payload.c_str(), t);
            if (s == nullptr) {
                send_close(0, p->msg.arg0, t);
            } else {
                s->peer = create_remote_socket(p->msg.arg0, t);
                s->peer->peer = s;
                send_ready(s->id, s->peer->id, t);
                s->ready(s);
            }
        }
        break;

    case A_OKAY: /* READY(local-id, remote-id, "") */
        if (t->online && p->msg.arg0 != 0 && p->msg.arg1 != 0) {
            asocket* s = find_local_socket(p->msg.arg1, 0);
            if (s) {
                if(s->peer == 0) {
                    /* On first READY message, create the connection. */
                    s->peer = create_remote_socket(p->msg.arg0, t);
                    s->peer->peer = s;
                    s->ready(s);
                } else if (s->peer->id == p->msg.arg0) {
                    /* Other READY messages must use the same local-id */
                    s->ready(s);
                } else {
                    D("Invalid A_OKAY(%d,%d), expected A_OKAY(%d,%d) on transport %s",
                      p->msg.arg0, p->msg.arg1, s->peer->id, p->msg.arg1, t->serial);
                }
            } else {
                // When receiving A_OKAY from device for A_OPEN request, the host server may
                // have closed the local socket because of client disconnection. Then we need
                // to send A_CLSE back to device to close the service on device.
                send_close(p->msg.arg1, p->msg.arg0, t);
            }
        }
        break;

    case A_CLSE: /* CLOSE(local-id, remote-id, "") or CLOSE(0, remote-id, "") */
        if (t->online && p->msg.arg1 != 0) {
            asocket* s = find_local_socket(p->msg.arg1, p->msg.arg0);
            if (s) {
                /* According to protocol.txt, p->msg.arg0 might be 0 to indicate
                 * a failed OPEN only. However, due to a bug in previous ADB
                 * versions, CLOSE(0, remote-id, "") was also used for normal
                 * CLOSE() operations.
                 *
                 * This is bad because it means a compromised adbd could
                 * send packets to close connections between the host and
                 * other devices. To avoid this, only allow this if the local
                 * socket has a peer on the same transport.
                 */
                if (p->msg.arg0 == 0 && s->peer && s->peer->transport != t) {
                    D("Invalid A_CLSE(0, %u) from transport %s, expected transport %s",
                      p->msg.arg1, t->serial, s->peer->transport->serial);
                } else {
                    s->close(s);
                }
            }
        }
        break;

    case A_WRTE: /* WRITE(local-id, remote-id, <data>) */
        if (t->online && p->msg.arg0 != 0 && p->msg.arg1 != 0) {
            asocket* s = find_local_socket(p->msg.arg1, p->msg.arg0);
            if (s) {
                unsigned rid = p->msg.arg0;
                if (s->enqueue(s, std::move(p->payload)) == 0) {
                    D("Enqueue the socket");
                    send_ready(s->id, rid, t);
                }
            }
        }
        break;

    default:
        printf("handle_packet: what is %08x?!\n", p->msg.command);
    }

    put_apacket(p);
}












/**
 * void usb_init()
 * --> static void usb_ffs_init()
 * ---> static void usb_ffs_open_thread(void* x)
 * ----> register_usb_transport(usb, 0, 0, 1);
*/
void register_usb_transport(usb_handle* usb, const char* serial, const char* devpath,unsigned writeable) {
    atransport* t = new atransport((writeable ? kCsOffline : kCsNoPerm));

    D("transport: %p init'ing for usb_handle %p (sn='%s')", t, usb, serial ? serial : "");
    /**
     * @    system/core/adb/transport_usb.cpp
    */
    init_usb_transport(t, usb);
    if (serial) {
        t->serial = strdup(serial);
    }

    if (devpath) {
        t->devpath = strdup(devpath);
    }

    {
        std::lock_guard<std::recursive_mutex> lock(transport_lock);
        pending_list.push_front(t);
    }

    register_transport(t);
}

/**
 * @    system/core/adb/transport_usb.cpp
*/
void init_usb_transport(atransport* t, usb_handle* h) {
    D("transport: usb");
    t->connection.reset(new UsbConnection(h));
    t->sync_token = 1;
    t->type = kTransportUsb;
}

/* the fdevent select pump is single threaded */
static void register_transport(atransport* transport) {
    tmsg m;
    m.transport = transport;
    m.action = 1;
    D("transport: %s registered", transport->serial);
    if (transport_write_action(transport_registration_send, &m)) {
        fatal_errno("cannot write transport registration socket\n");
    }
}

static int transport_write_action(int fd, struct tmsg* m) {
    char* p = (char*)m;
    int len = sizeof(*m);
    int r;

    while (len > 0) {
        r = adb_write(fd, p, len);
        if (r > 0) {
            len -= r;
            p += r;
        } else {
            D("transport_write_action: on fd %d: %s", fd, strerror(errno));
            return -1;
        }
    }
    return 0;
}



















