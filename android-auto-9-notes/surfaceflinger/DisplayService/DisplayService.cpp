
/*

R Interface                                                                                                                             Thread Use Server Clients
Y android.frameworks.displayservice@1.0::IDisplayService/default            0/1        1568   1557


*/





//  @   frameworks/hardware/interfaces/displayservice/1.0/IDisplayService.hal
package android.frameworks.displayservice@1.0;

import IDisplayEventReceiver;

interface IDisplayService {
    /**
     * Must create new receiver.
     *
     * @return receiver Receiver object.
     */
    getEventReceiver() generates(IDisplayEventReceiver receiver);
};









//  @   frameworks/native/services/displayservice/DisplayService.cpp

Return<sp<IDisplayEventReceiver>> DisplayService::getEventReceiver() {
    return new DisplayEventReceiver();
}