#ifndef _FLASH_SERVICE_H
#define _FLASH_SERVICE_H

class NetlinkEvent;

class FlashService {
   public:
    static FlashService* Instance();
    virtual ~FlashService();

    int start();
    int stop();

    int handleHidrawEvent(NetlinkEvent* evt);

   private:
    static FlashService* sInstance;

    FlashService();

    int isScannerGun();
    void launchApp();
};

#endif  // _FLASH_SERVICE_H
