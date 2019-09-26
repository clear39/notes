#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sysutils/NetlinkEvent.h>

#include "log.h"

#include "FlashService.h"

#include "NetlinkHandler.h"

NetlinkHandler::NetlinkHandler(int listenerSocket) : NetlinkListener(listenerSocket) {
}

NetlinkHandler::~NetlinkHandler() {
}

int NetlinkHandler::start() {
    return this->startListener();
}

int NetlinkHandler::stop() {
    return this->stopListener();
}

void NetlinkHandler::onEvent(NetlinkEvent *evt) {
    const char *subsys = evt->getSubsystem();

    if (!subsys) {
        ALOGW("No subsystem found in netlink event");
        return;
    }

    FlashService *fs = FlashService::Instance();

    if (!strncmp(subsys, "hidraw", 6)) {
        if (NULL != fs)
            fs->handleHidrawEvent(evt);
    }
}
