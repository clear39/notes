
//  @   frameworks/native/services/displayservice/DisplayService.cpp

Return<sp<IDisplayEventReceiver>> DisplayService::getEventReceiver() {
    return new DisplayEventReceiver();
}