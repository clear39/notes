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

        std::thread(write_transport_thread, t).detach();
        std::thread(read_transport_thread, t).detach();
    }

    {
        std::lock_guard<std::recursive_mutex> lock(transport_lock);
        pending_list.remove(t);
        transport_list.push_front(t);
    }

    update_transports();
}


static void transport_socket_events(int fd, unsigned events, void* _t) {
    atransport* t = reinterpret_cast<atransport*>(_t);
    D("transport_socket_events(fd=%d, events=%04x,...)", fd, events);
    if (events & FDE_READ) {
        apacket* p = 0;
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



















