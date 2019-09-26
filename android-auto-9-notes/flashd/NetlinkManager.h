#ifndef _NETLINKMANAGER_H
#define _NETLINKMANAGER_H

#include <sysutils/NetlinkListener.h>
#include <sysutils/SocketListener.h>

class NetlinkHandler;

class NetlinkManager {
   private:
    static NetlinkManager *sInstance;

   private:
    NetlinkHandler *mHandler;
    int mSock;

   public:
    virtual ~NetlinkManager();

    int start();
    int stop();

    static NetlinkManager *Instance();

   private:
    NetlinkManager();
};

#endif  // _NETLINKMANAGER_H
