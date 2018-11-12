
/**
 * Server socket class for zygote processes.
 *
 * Provides functions to wait for commands on a UNIX domain socket, and fork
 * off child processes that inherit the initial state of the VM.%
 *
 * Please see {@link ZygoteConnection.Arguments} for documentation on the
 * client protocol.
 */
class ZygoteServer {




	  /**
     * Registers a server socket for zygote command connections
     *
     * @throws RuntimeException when open fails
     */
	 //socketName = "zygote";
    void registerServerSocket(String socketName) {
        if (mServerSocket == null) {
            int fileDesc;
            final String fullSocketName = ANDROID_SOCKET_PREFIX + socketName; //		fullSocketName = "ANDROID_SOCKET_zygote"
            try {
                String env = System.getenv(fullSocketName);
                fileDesc = Integer.parseInt(env);
            } catch (RuntimeException ex) {
                throw new RuntimeException(fullSocketName + " unset or invalid", ex);
            }

            try {
                FileDescriptor fd = new FileDescriptor();
                fd.setInt$(fileDesc);
                mServerSocket = new LocalServerSocket(fd);
            } catch (IOException ex) {
                throw new RuntimeException("Error binding to local socket '" + fileDesc + "'", ex);
            }
        }
    }




    /**
     * Runs the zygote process's select loop. Accepts new connections as
     * they happen, and reads commands from connections one spawn-request's
     * worth at a time.
     *
     * @throws Zygote.MethodAndArgsCaller in a child process when a main()
     * should be executed.
     */
    void runSelectLoop(String abiList="armeabi-v7a,armeabi") throws Zygote.MethodAndArgsCaller {
        ArrayList<FileDescriptor> fds = new ArrayList<FileDescriptor>();
        ArrayList<ZygoteConnection> peers = new ArrayList<ZygoteConnection>();

        fds.add(mServerSocket.getFileDescriptor());
        peers.add(null);

        while (true) {
            StructPollfd[] pollFds = new StructPollfd[fds.size()];
            for (int i = 0; i < pollFds.length; ++i) {
                pollFds[i] = new StructPollfd();
                pollFds[i].fd = fds.get(i);
                pollFds[i].events = (short) POLLIN;
            }
            try {
                Os.poll(pollFds, -1);
            } catch (ErrnoException ex) {
                throw new RuntimeException("poll failed", ex);
            }
            for (int i = pollFds.length - 1; i >= 0; --i) {
                if ((pollFds[i].revents & POLLIN) == 0) {
                    continue;
                }
                if (i == 0) {//这里i == 0是zygote套接字，则表示有客户端连接zygote套接字服务端
                    ZygoteConnection newPeer = acceptCommandPeer(abiList);  //abiList = "armeabi-v7a,armeabi"
                    peers.add(newPeer);
                    fds.add(newPeer.getFileDesciptor());
                } else {
                    boolean done = peers.get(i).runOnce(this); //ZygoteConnection
                    if (done) {//true表示子进程创建成功，移除客户端相关记录信息
                        peers.remove(i);
                        fds.remove(i);
                    }
                }
            }
        }
    }


    /**
     * Waits for and accepts a single command connection. Throws
     * RuntimeException on failure.
     */
    private ZygoteConnection acceptCommandPeer(String abiList) {
        try {
            return createNewConnection(mServerSocket.accept(), abiList);
        } catch (IOException ex) {
            throw new RuntimeException("IOException during accept()", ex);
        }
    }


     protected ZygoteConnection createNewConnection(LocalSocket socket, String abiList) throws IOException {
        return new ZygoteConnection(socket, abiList);
    }

}