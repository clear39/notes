
/***
 * @   frameworks/av/services/audiopolicy/common/managerdefinitions/include/SessionRoute.h:76:
 * class SessionRouteMap: public KeyedVector<audio_session_t, sp<SessionRoute> >
 * 
 * */




//  @   frameworks/av/services/audiopolicy/common/managerdefinitions/src/SessionRoute.cpp



void SessionRouteMap::addRoute(audio_session_t session,
                               audio_stream_type_t streamType,
                               audio_source_t source,
                               const sp<DeviceDescriptor>& descriptor,
                               uid_t uid)
{
    if (mMapType == MAPTYPE_INPUT && streamType != SessionRoute::STREAM_TYPE_NA) {
        ALOGE("Adding Output Route to InputRouteMap");
        return;
    } else if (mMapType == MAPTYPE_OUTPUT && source != SessionRoute::SOURCE_TYPE_NA) {
        ALOGE("Adding Input Route to OutputRouteMap");
        return;
    }

    sp<SessionRoute> route = indexOfKey(session) >= 0 ? valueFor(session) : 0;

    if (route != 0) {
        if (descriptor != 0 || route->mDeviceDescriptor != 0) {
            route->mChanged = true;
        }
        route->mRefCount++;
        route->mDeviceDescriptor = descriptor;
    } else {
        route = new SessionRoute(session, streamType, source, descriptor, uid);
        route->mRefCount++;
        if (descriptor != 0) {
            route->mChanged = true;
        }
        add(session, route);
    }
}




audio_devices_t SessionRouteMap::getActiveDeviceForStream(audio_stream_type_t streamType,const DeviceVector& availableDevices)
{
    for (size_t index = 0; index < size(); index++) {
        sp<SessionRoute> route = valueAt(index);
        if (streamType == route->mStreamType && route->isActiveOrChanged() && route->mDeviceDescriptor != 0) {
            audio_devices_t device = route->mDeviceDescriptor->type();
            if (!availableDevices.getDevicesFromType(device).isEmpty()) {
                return device;
            }
        }
    }
    return AUDIO_DEVICE_NONE;
}



