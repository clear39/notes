/** @hide */
public MediaRouter(Context context) {
	synchronized (Static.class) {
	    if (sStatic == null) {
		final Context appContext = context.getApplicationContext();
		sStatic = new Static(appContext);
		sStatic.startMonitoringRoutes(appContext);
	    }
	}
}


Static(Context appContext) {
    mPackageName = appContext.getPackageName();
    mResources = appContext.getResources();
    mHandler = new Handler(appContext.getMainLooper());

    IBinder b = ServiceManager.getService(Context.AUDIO_SERVICE);
    mAudioService = IAudioService.Stub.asInterface(b);

    mDisplayService = (DisplayManager) appContext.getSystemService(Context.DISPLAY_SERVICE);

    mMediaRouterService = IMediaRouterService.Stub.asInterface(ServiceManager.getService(Context.MEDIA_ROUTER_SERVICE));

    mSystemCategory = new RouteCategory(com.android.internal.R.string.default_audio_route_category_name,ROUTE_TYPE_LIVE_AUDIO | ROUTE_TYPE_LIVE_VIDEO, false);
    mSystemCategory.mIsSystem = true;

    // Only the system can configure wifi displays.  The display manager
    // enforces this with a permission check.  Set a flag here so that we
    // know whether this process is actually allowed to scan and connect.
    mCanConfigureWifiDisplays = appContext.checkPermission(Manifest.permission.CONFIGURE_WIFI_DISPLAY,Process.myPid(), Process.myUid()) == PackageManager.PERMISSION_GRANTED;
}


// Called after sStatic is initialized
void startMonitoringRoutes(Context appContext) {
    mDefaultAudioVideo = new RouteInfo(mSystemCategory);
    mDefaultAudioVideo.mNameResId = com.android.internal.R.string.default_audio_route_name;
    mDefaultAudioVideo.mSupportedTypes = ROUTE_TYPE_LIVE_AUDIO | ROUTE_TYPE_LIVE_VIDEO;
    mDefaultAudioVideo.updatePresentationDisplay();
    if (((AudioManager) appContext.getSystemService(Context.AUDIO_SERVICE)).isVolumeFixed()) {
        mDefaultAudioVideo.mVolumeHandling = RouteInfo.PLAYBACK_VOLUME_FIXED;
    }

    addRouteStatic(mDefaultAudioVideo);
    mSystemAudioRoute = mDefaultAudioVideo;

    // This will select the active wifi display route if there is one.
    updateWifiDisplayStatus(mDisplayService.getWifiDisplayStatus());

    appContext.registerReceiver(new WifiDisplayStatusChangedReceiver(),new IntentFilter(DisplayManager.ACTION_WIFI_DISPLAY_STATUS_CHANGED));
    appContext.registerReceiver(new VolumeChangeReceiver(),new IntentFilter(AudioManager.VOLUME_CHANGED_ACTION));

    mDisplayService.registerDisplayListener(this, mHandler);

    AudioRoutesInfo newAudioRoutes = null;
    try {
        newAudioRoutes = mAudioService.startWatchingRoutes(mAudioRoutesObserver);
    } catch (RemoteException e) {
    }
    if (newAudioRoutes != null) {
        // This will select the active BT route if there is one and the current
        // selected route is the default system route, or if there is no selected
        // route yet.
        updateAudioRoutes(newAudioRoutes);
    }

    // Bind to the media router service.
    rebindAsUser(UserHandle.myUserId());

    // Select the default route if the above didn't sync us up
    // appropriately with relevant system state.
    if (mSelectedRoute == null) {
        selectDefaultRouteStatic();
    }
}


static void addRouteStatic(RouteInfo info) {
        Log.v(TAG, "Adding route: " + info);
        final RouteCategory cat = info.getCategory();
        if (!sStatic.mCategories.contains(cat)) {
            sStatic.mCategories.add(cat);
        }
        if (cat.isGroupable() && !(info instanceof RouteGroup)) {
            // Enforce that any added route in a groupable category must be in a group.
            final RouteGroup group = new RouteGroup(info.getCategory());
            group.mSupportedTypes = info.mSupportedTypes;
            sStatic.mRoutes.add(group);
            dispatchRouteAdded(group);
            group.addRoute(info);

            info = group;
        } else {
            sStatic.mRoutes.add(info);
            dispatchRouteAdded(info);
        }
}

