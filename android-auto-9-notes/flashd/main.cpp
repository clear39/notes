#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"

#include "FlashService.h"

#include "NetlinkManager.h"


int main(int argc, char** argv) {

    setenv("ANDROID_LOG_TAGS", "*:v", 1);

    FlashService *fs;
    NetlinkManager *nm;

    ALOGD("-> Flashd start");

    /* Create our singleton managers */
    if (!(fs = FlashService::Instance())) {
        ALOGE("Unable to create FlashService");
        exit(1);
    }

    if (!(nm = NetlinkManager::Instance())) {
        ALOGE("Unable to create NetlinkManager");
        exit(1);
    };

    if (nm->start()) {
        ALOGE("Unable to start NetlinkManager (%s)", strerror(errno));
        exit(1);
    }

    if (fs->start()) {
        ALOGE("Unable to start FlashService (%s)", strerror(errno));
        exit(1);
    }

    // Eventually we'll become the monitoring thread
    while (1)
        usleep(1000);


    ALOGW("-> Flashd stop");

    return 0;
}
