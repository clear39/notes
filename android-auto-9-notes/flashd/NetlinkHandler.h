#ifndef _NETLINKHANDLER_H
#define _NETLINKHANDLER_H

#include <sysutils/NetlinkListener.h>

class NetlinkHandler : public NetlinkListener {
   public:
    NetlinkHandler(int listenerSocket);
    virtual ~NetlinkHandler();

    int start(void);
    int stop(void);

   protected:
    virtual void onEvent(NetlinkEvent *evt);
};

#endif  // _NETLINKHANDLER_H
