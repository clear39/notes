//  @/work/workcodes/aosp-p9.x-auto-beta/system/core/init/service.cpp
ServiceList::ServiceList() {}

ServiceList& ServiceList::GetInstance() {
    static ServiceList instance;
    return instance;
}