//	@frameworks/av/media/libmediametrics/MediaAnalyticsItem.cpp
MediaAnalyticsItem::MediaAnalyticsItem(MediaAnalyticsItem::Key key /*= kKeyPlayer*/)
    : mPid(-1),
      mUid(-1),
      mPkgVersionCode(0),
      mSessionID(MediaAnalyticsItem::SessionIDNone),
      mTimestamp(0),
      mFinalized(0),
      mPropCount(0), mPropSize(0), mProps(NULL)
{
    if (DEBUG_ALLOCATIONS) {
        ALOGD("Allocate MediaAnalyticsItem @ %p", this);
    }
    mKey = key;
}