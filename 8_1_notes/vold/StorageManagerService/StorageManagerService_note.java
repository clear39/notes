//	@base/services/core/java/com/android/server/StorageManagerService.java
class StorageManagerService extends IStorageManager.Stub implements INativeDaemonConnectorCallbacks, Watchdog.Monitor {

	/**
     * Constructs a new StorageManagerService instance
     *
     * @param context  Binder context for this service
     */
    public StorageManagerService(Context context) {
        sSelf = this;

        mContext = context;
        mCallbacks = new Callbacks(FgThread.get().getLooper());
        mLockPatternUtils = new LockPatternUtils(mContext);

        // XXX: This will go away soon in favor of IMountServiceObserver
        mPms = (PackageManagerService) ServiceManager.getService("package");

        HandlerThread hthread = new HandlerThread(TAG);
        hthread.start();
        mHandler = new StorageManagerServiceHandler(hthread.getLooper());

        // Add OBB Action Handler to StorageManagerService thread.
        mObbActionHandler = new ObbActionHandler(IoThread.get().getLooper());

        // Initialize the last-fstrim tracking if necessary
        File dataDir = Environment.getDataDirectory();
        File systemDir = new File(dataDir, "system");
        //	/data/system/last-fstrim
        mLastMaintenanceFile = new File(systemDir, LAST_FSTRIM_FILE);//	private static final String LAST_FSTRIM_FILE = "last-fstrim";
        if (!mLastMaintenanceFile.exists()) {
            // Not setting mLastMaintenance here means that we will force an
            // fstrim during reboot following the OTA that installs this code.
            try {
                (new FileOutputStream(mLastMaintenanceFile)).close();
            } catch (IOException e) {
                Slog.e(TAG, "Unable to create fstrim record " + mLastMaintenanceFile.getPath());
            }
        } else {
            mLastMaintenance = mLastMaintenanceFile.lastModified();
        }

        mSettingsFile = new AtomicFile(new File(Environment.getDataSystemDirectory(), "storage.xml"));//	/data/system/storage.xml
        synchronized (mLock) {
            readSettingsLocked();//读取/data/system/storage.xml文件
        }

        //	LocalServices	@base/core/java/com/android/server/LocalServices.java
        //	private final StorageManagerInternalImpl mStorageManagerInternal = new StorageManagerInternalImpl();
        LocalServices.addService(StorageManagerInternal.class, mStorageManagerInternal);

        /*
         * Create the connection to vold with a maximum queue of twice the
         * amount of containers we'd ever expect to have. This keeps an
         * "asec list" from blocking a thread repeatedly.
         */

        mConnector = new NativeDaemonConnector(this, "vold", MAX_CONTAINERS * 2, VOLD_TAG, 25,null);
        mConnector.setDebug(true);
        mConnector.setWarnIfHeld(mLock);
        mConnectorThread = new Thread(mConnector, VOLD_TAG);

        // Reuse parameters from first connector since they are tested and safe
        mCryptConnector = new NativeDaemonConnector(this, "cryptd",MAX_CONTAINERS * 2, CRYPTD_TAG, 25, null);
        mCryptConnector.setDebug(true);
        mCryptConnectorThread = new Thread(mCryptConnector, CRYPTD_TAG);

        final IntentFilter userFilter = new IntentFilter();
        userFilter.addAction(Intent.ACTION_USER_ADDED);
        userFilter.addAction(Intent.ACTION_USER_REMOVED);
        mContext.registerReceiver(mUserReceiver, userFilter, null, mHandler);

        synchronized (mLock) {
            addInternalVolumeLocked();
        }

        // Add ourself to the Watchdog monitors if enabled.
        if (WATCHDOG_ENABLE) {
            Watchdog.getInstance().addMonitor(this); 
        }
    }

    private void readSettingsLocked() {
        mRecords.clear();
        mPrimaryStorageUuid = getDefaultPrimaryStorageUuid();
        mForceAdoptable = false;

        FileInputStream fis = null;
        try {
            fis = mSettingsFile.openRead();
            final XmlPullParser in = Xml.newPullParser();
            in.setInput(fis, StandardCharsets.UTF_8.name());

            int type;
            while ((type = in.next()) != END_DOCUMENT) {
                if (type == START_TAG) {
                    final String tag = in.getName();
                    if (TAG_VOLUMES.equals(tag)) {
                        final int version = readIntAttribute(in, ATTR_VERSION, VERSION_INIT);
                        final boolean primaryPhysical = SystemProperties.getBoolean(
                                StorageManager.PROP_PRIMARY_PHYSICAL, false);
                        final boolean validAttr = (version >= VERSION_FIX_PRIMARY)
                                || (version >= VERSION_ADD_PRIMARY && !primaryPhysical);
                        if (validAttr) {
                            mPrimaryStorageUuid = readStringAttribute(in,
                                    ATTR_PRIMARY_STORAGE_UUID);
                        }
                        mForceAdoptable = readBooleanAttribute(in, ATTR_FORCE_ADOPTABLE, false);

                    } else if (TAG_VOLUME.equals(tag)) {
                        final VolumeRecord rec = readVolumeRecord(in);
                        mRecords.put(rec.fsUuid, rec);
                    }
                }
            }
        } catch (FileNotFoundException e) {
            // Missing metadata is okay, probably first boot
        } catch (IOException e) {
            Slog.wtf(TAG, "Failed reading metadata", e);
        } catch (XmlPullParserException e) {
            Slog.wtf(TAG, "Failed reading metadata", e);
        } finally {
            IoUtils.closeQuietly(fis);
        }
    }

    private String getDefaultPrimaryStorageUuid() {
        //  public static final String PROP_PRIMARY_PHYSICAL = "ro.vold.primary_physical"; //这里为null
        if (SystemProperties.getBoolean(StorageManager.PROP_PRIMARY_PHYSICAL, false)) {
            return StorageManager.UUID_PRIMARY_PHYSICAL;    //public static final String UUID_PRIMARY_PHYSICAL = "primary_physical";
        } else {
            return StorageManager.UUID_PRIVATE_INTERNAL;//  public static final String UUID_PRIVATE_INTERNAL = null;
        }
    }


    private void addInternalVolumeLocked() {
        // Create a stub volume that represents internal storage
        //  public static final String ID_PRIVATE_INTERNAL = "private";
        //  public static final int TYPE_PRIVATE = 1;
        final VolumeInfo internal = new VolumeInfo(VolumeInfo.ID_PRIVATE_INTERNAL,VolumeInfo.TYPE_PRIVATE, null, null);
        internal.state = VolumeInfo.STATE_MOUNTED;
        internal.path = Environment.getDataDirectory().getAbsolutePath();
        mVolumes.put(internal.id, internal);
    }

    private void start() {
        mConnectorThread.start();
        mCryptConnectorThread.start();
    }




    /**
    //不能挂载
      2018-11-28T09:42:20.983 - RCV <- {640 disk:8,0 8}
	  2018-11-28T09:42:20.988 - RCV <- {641 disk:8,0 16025387008}
	  2018-11-28T09:42:20.989 - RCV <- {642 disk:8,0 General}
	  2018-11-28T09:42:20.989 - RCV <- {644 disk:8,0 /sys//devices/platform/5b110000.cdns3/xhci-cdns3/usb1/1-1/1-1.3/1-1.3:1.0/host0/target0:0:0/0:0:0:0/block/sda}
	  2018-11-28T09:42:21.034 - RCV <- {650 public:8,1 0 "disk:8,0" ""}
	  2018-11-28T09:42:21.034 - RCV <- {651 public:8,1 0}

	  2018-11-28T09:42:21.035 - SND -> {7 volume mount public:8,1 0 0}

	  2018-11-28T09:42:21.035 - RCV <- {643 disk:8,0}
	  2018-11-28T09:42:21.035 - RCV <- {651 public:8,1 1}
	  2018-11-28T09:42:21.113 - RCV <- {652 public:8,1 vfat}
	  2018-11-28T09:42:21.114 - RCV <- {653 public:8,1 5243-5977}
	  2018-11-28T09:42:21.115 - RCV <- {654 public:8,1 }
	  2018-11-28T09:42:21.687 - RCV <- {656 public:8,1 /mnt/media_rw/5243-5977}
	  2018-11-28T09:42:21.688 - RCV <- {655 public:8,1 /mnt/media_rw/5243-5977}            //这里是唯一区别
	  2018-11-28T09:42:21.710 - RCV <- {651 public:8,1 2}
	  2018-11-28T09:42:21.711 - RCV <- {200 7 Command succeeded}
	  2018-11-28T09:42:21.712 - NDC Command {7 volume mount public:8,1 0 0} took too long (677ms)





    //正常挂载
      2018-11-28T14:42:16.309 - RCV <- {640 disk:8,0 8}
      2018-11-28T14:42:16.311 - RCV <- {641 disk:8,0 16025387008}
      2018-11-28T14:42:16.312 - RCV <- {642 disk:8,0 General }
      2018-11-28T14:42:16.313 - RCV <- {644 disk:8,0 /sys//devices/platform/5b110000.cdns3/xhci-cdns3/usb1/1-1/1-1.3/1-1.3:1.0/host0/target0:0:0/0:0:0:0/block/sda}
      2018-11-28T14:42:16.353 - RCV <- {650 public:8,1 0 "disk:8,0" ""}
      2018-11-28T14:42:16.354 - RCV <- {651 public:8,1 0}
      2018-11-28T14:42:16.354 - SND -> {11 volume mount public:8,1 2 0}
      2018-11-28T14:42:16.354 - RCV <- {643 disk:8,0}
      2018-11-28T14:42:16.355 - RCV <- {651 public:8,1 1}
      2018-11-28T14:42:16.421 - RCV <- {652 public:8,1 vfat}
      2018-11-28T14:42:16.422 - RCV <- {653 public:8,1 5243-5977}
      2018-11-28T14:42:16.422 - RCV <- {654 public:8,1 }
      2018-11-28T14:42:16.948 - RCV <- {656 public:8,1 /mnt/media_rw/5243-5977}
      2018-11-28T14:42:16.949 - RCV <- {655 public:8,1 /storage/5243-5977}                      //这里是唯一区别
      2018-11-28T14:42:17.084 - RCV <- {651 public:8,1 2}
      2018-11-28T14:42:17.085 - RCV <- {200 11 Command succeeded}
      2018-11-28T14:42:17.086 - NDC Command {11 volume mount public:8,1 2 0} took too long (731ms)

    */


    /**
     * Callback from NativeDaemonConnector
     */
    @Override
    public boolean onEvent(int code, String raw, String[] cooked) {
        synchronized (mLock) {
            return onEventLocked(code, raw, cooked);
        }
    }

    private boolean onEventLocked(int code, String raw, String[] cooked) {
        switch (code) {
            case VoldResponseCode.DISK_CREATED: {//1	2018-11-28T09:42:20.983 - RCV <- {640 disk:8,0 8}
                if (cooked.length != 3) break;
                final String id = cooked[1];
                int flags = Integer.parseInt(cooked[2]);
                //	public static final String PROP_FORCE_ADOPTABLE = "persist.fw.force_adoptable"; //这里null
                //  mForceAdoptable 可以通过dumpsys mount 命令查询到
                if (SystemProperties.getBoolean(StorageManager.PROP_FORCE_ADOPTABLE, false) || mForceAdoptable) {
                    flags |= DiskInfo.FLAG_ADOPTABLE;
                }
                // Adoptable storage isn't currently supported on FBE devices
                //	public static final String PROP_ADOPTABLE_FBE = "persist.sys.adoptable_fbe";//这里null
                //  StorageManager.isFileEncryptedNativeOnly() 返回 true 
                if (StorageManager.isFileEncryptedNativeOnly() && !SystemProperties.getBoolean(StorageManager.PROP_ADOPTABLE_FBE, false)) {
                    //这里执行
                    flags &= ~DiskInfo.FLAG_ADOPTABLE;  //public static final int FLAG_ADOPTABLE = 1 << 0;  //这里有flags为8，没有实际的作用
                }
                mDisks.put(id, new DiskInfo(id, flags));// id = "disk:8,0" ,flags = 8
                break;
            }
            case VoldResponseCode.DISK_SIZE_CHANGED: {//2    2018-11-28T09:42:20.988 - RCV <- {641 disk:8,0 16025387008}
                if (cooked.length != 3) break;
                final DiskInfo disk = mDisks.get(cooked[1]);// disk:8,0
                if (disk != null) {
                    disk.size = Long.parseLong(cooked[2]);  //16025387008 
                }
                break;
            }
            case VoldResponseCode.DISK_LABEL_CHANGED: {//3   2018-11-28T09:42:20.989 - RCV <- {642 disk:8,0 General}
                final DiskInfo disk = mDisks.get(cooked[1]);    //  disk:8,0
                if (disk != null) {
                    final StringBuilder builder = new StringBuilder();
                    for (int i = 2; i < cooked.length; i++) {
                        builder.append(cooked[i]).append(' ');
                    }
                    disk.label = builder.toString().trim(); // "General"
                }
                break;
            }
            //7  2018-11-28T09:42:21.035 - RCV <- {643 disk:8,0}
            case VoldResponseCode.DISK_SCANNED: {
                if (cooked.length != 2) break;
                final DiskInfo disk = mDisks.get(cooked[1]);
                if (disk != null) {
                    onDiskScannedLocked(disk);
                }
                break;
            }
            //4  2018-11-28T09:42:20.989 - RCV <- {644 disk:8,0 /sys//devices/platform/5b110000.cdns3/xhci-cdns3/usb1/1-1/1-1.3/1-1.3:1.0/host0/target0:0:0/0:0:0:0/block/sda}
            case VoldResponseCode.DISK_SYS_PATH_CHANGED: {
                if (cooked.length != 3) break;
                final DiskInfo disk = mDisks.get(cooked[1]);
                if (disk != null) {
                    //  /sys//devices/platform/5b110000.cdns3/xhci-cdns3/usb1/1-1/1-1.3/1-1.3:1.0/host0/target0:0:0/0:0:0:0/block/sda
                    disk.sysPath = cooked[2];
                }
                break;
            }
            case VoldResponseCode.DISK_DESTROYED: {
                if (cooked.length != 2) break;
                final DiskInfo disk = mDisks.remove(cooked[1]);
                if (disk != null) {
                    mCallbacks.notifyDiskDestroyed(disk);
                }
                break;
            }
            //5   2018-11-28T09:42:21.034 - RCV <- {650 public:8,1 0 "disk:8,0" ""}
            case VoldResponseCode.VOLUME_CREATED: {
                final String id = cooked[1];
                final int type = Integer.parseInt(cooked[2]);
                final String diskId = TextUtils.nullIfEmpty(cooked[3]);
                final String partGuid = TextUtils.nullIfEmpty(cooked[4]);

                final DiskInfo disk = mDisks.get(diskId);
                final VolumeInfo vol = new VolumeInfo(id, type, disk, partGuid);
                mVolumes.put(id, vol);
                onVolumeCreatedLocked(vol);//这里会发送 2018-11-28T09:42:21.035 - SND -> {7 volume mount public:8,1 0 0} 给vold挂在
                break;
            }
            //6 2018-11-28T09:42:21.034 - RCV <- {651 public:8,1 0}    这一第一次只是发送一个状态改变的广播 
            //8 2018-11-28T09:42:21.035 - RCV <- {651 public:8,1 1}
            //14  2018-11-28T09:42:21.710 - RCV <- {651 public:8,1 2}
            case VoldResponseCode.VOLUME_STATE_CHANGED: {
                if (cooked.length != 3) break;
                final VolumeInfo vol = mVolumes.get(cooked[1]);
                if (vol != null) {
                    final int oldState = vol.state;
                    final int newState = Integer.parseInt(cooked[2]);
                    vol.state = newState;
                    onVolumeStateChangedLocked(vol, oldState, newState);
                }
                break;
            }
            //9  2018-11-28T09:42:21.113 - RCV <- {652 public:8,1 vfat}
            case VoldResponseCode.VOLUME_FS_TYPE_CHANGED: {
                if (cooked.length != 3) break;
                final VolumeInfo vol = mVolumes.get(cooked[1]);
                if (vol != null) {
                    vol.fsType = cooked[2];//vfat
                }
                break;
            }
            //10  2018-11-28T09:42:21.114 - RCV <- {653 public:8,1 5243-5977}
            case VoldResponseCode.VOLUME_FS_UUID_CHANGED: {
                if (cooked.length != 3) break;
                final VolumeInfo vol = mVolumes.get(cooked[1]);
                if (vol != null) {
                    vol.fsUuid = cooked[2];//   5243-5977
                }
                break;
            }
            //11    2018-11-28T09:42:21.115 - RCV <- {654 public:8,1 }
            case VoldResponseCode.VOLUME_FS_LABEL_CHANGED: {
                final VolumeInfo vol = mVolumes.get(cooked[1]);
                if (vol != null) {
                    final StringBuilder builder = new StringBuilder();
                    for (int i = 2; i < cooked.length; i++) {
                        builder.append(cooked[i]).append(' ');
                    }
                    vol.fsLabel = builder.toString().trim();//这里为null 
                }
                // TODO: notify listeners that label changed
                break;
            }

            //13    2018-11-28T14:42:16.949 - RCV <- {655 public:8,1 /storage/5243-5977}
            case VoldResponseCode.VOLUME_PATH_CHANGED: {
                if (cooked.length != 3) break;
                final VolumeInfo vol = mVolumes.get(cooked[1]);
                if (vol != null) {
                    vol.path = cooked[2];   //  /mnt/media_rw/5243-5977
                }
                break;
            }

            //12  2018-11-28T09:42:21.687 - RCV <- {656 public:8,1 /mnt/media_rw/5243-5977}
            case VoldResponseCode.VOLUME_INTERNAL_PATH_CHANGED: {
                if (cooked.length != 3) break;
                final VolumeInfo vol = mVolumes.get(cooked[1]);
                if (vol != null) {
                    vol.internalPath = cooked[2];// /mnt/media_rw/5243-5977
                }
                break;
            }
            case VoldResponseCode.VOLUME_DESTROYED: {
                if (cooked.length != 2) break;
                mVolumes.remove(cooked[1]);
                break;
            }

            case VoldResponseCode.MOVE_STATUS: {
                final int status = Integer.parseInt(cooked[1]);
                onMoveStatusLocked(status);
                break;
            }
            case VoldResponseCode.BENCHMARK_RESULT: {
                if (cooked.length != 7) break;
                final String path = cooked[1];
                final String ident = cooked[2];
                final long create = Long.parseLong(cooked[3]);
                final long drop = Long.parseLong(cooked[4]);
                final long run = Long.parseLong(cooked[5]);
                final long destroy = Long.parseLong(cooked[6]);

                final DropBoxManager dropBox = mContext.getSystemService(DropBoxManager.class);
                dropBox.addText(TAG_STORAGE_BENCHMARK, scrubPath(path)
                        + " " + ident + " " + create + " " + run + " " + destroy);

                final VolumeRecord rec = findRecordForPath(path);
                if (rec != null) {
                    rec.lastBenchMillis = System.currentTimeMillis();
                    writeSettingsLocked();
                }

                break;
            }
            case VoldResponseCode.TRIM_RESULT: {
                if (cooked.length != 4) break;
                final String path = cooked[1];
                final long bytes = Long.parseLong(cooked[2]);
                final long time = Long.parseLong(cooked[3]);

                final DropBoxManager dropBox = mContext.getSystemService(DropBoxManager.class);
                dropBox.addText(TAG_STORAGE_TRIM, scrubPath(path)
                        + " " + bytes + " " + time);

                final VolumeRecord rec = findRecordForPath(path);
                if (rec != null) {
                    rec.lastTrimMillis = System.currentTimeMillis();
                    writeSettingsLocked();
                }

                break;
            }

            default: {
                Slog.d(TAG, "Unhandled vold event " + code);
            }
        }

        return true;
    }


    private void onDiskScannedLocked(DiskInfo disk) {
        int volumeCount = 0;
        for (int i = 0; i < mVolumes.size(); i++) {
            final VolumeInfo vol = mVolumes.valueAt(i);
            if (Objects.equals(disk.id, vol.getDiskId())) {
                volumeCount++;
            }
        }

        final Intent intent = new Intent(DiskInfo.ACTION_DISK_SCANNED);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT
                | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        intent.putExtra(DiskInfo.EXTRA_DISK_ID, disk.id);
        intent.putExtra(DiskInfo.EXTRA_VOLUME_COUNT, volumeCount);
        mHandler.obtainMessage(H_INTERNAL_BROADCAST, intent).sendToTarget();

        final CountDownLatch latch = mDiskScanLatches.remove(disk.id);
        if (latch != null) {
            latch.countDown();
        }

        disk.volumeCount = volumeCount;
        mCallbacks.notifyDiskScanned(disk, volumeCount);
    }


    private boolean isBroadcastWorthy(VolumeInfo vol) {
        switch (vol.getType()) {
            case VolumeInfo.TYPE_PRIVATE:
            case VolumeInfo.TYPE_PUBLIC:
            case VolumeInfo.TYPE_EMULATED:
                break;
            default:
                return false;
        }

        switch (vol.getState()) {
            case VolumeInfo.STATE_MOUNTED:
            case VolumeInfo.STATE_MOUNTED_READ_ONLY:
            case VolumeInfo.STATE_EJECTING:
            case VolumeInfo.STATE_UNMOUNTED:
            case VolumeInfo.STATE_UNMOUNTABLE:
            case VolumeInfo.STATE_BAD_REMOVAL:
                break;
            default:
                return false;
        }

        return true;
    }


    private void onVolumeStateChangedLocked(VolumeInfo vol, int oldState, int newState) {
        // Remember that we saw this volume so we're ready to accept user
        // metadata, or so we can annoy them when a private volume is ejected
        if (vol.isMountedReadable() && !TextUtils.isEmpty(vol.fsUuid)) {
            VolumeRecord rec = mRecords.get(vol.fsUuid);
            if (rec == null) {
                rec = new VolumeRecord(vol.type, vol.fsUuid);
                rec.partGuid = vol.partGuid;
                rec.createdMillis = System.currentTimeMillis();
                if (vol.type == VolumeInfo.TYPE_PRIVATE) {
                    rec.nickname = vol.disk.getDescription();
                }
                mRecords.put(rec.fsUuid, rec);
                writeSettingsLocked();
            } else {
                // Handle upgrade case where we didn't store partition GUID
                if (TextUtils.isEmpty(rec.partGuid)) {
                    rec.partGuid = vol.partGuid;
                    writeSettingsLocked();
                }
            }
        }

        //当有设置监听该service,回调
        mCallbacks.notifyVolumeStateChanged(vol, oldState, newState);

        // Do not broadcast before boot has completed to avoid launching the
        // processes that receive the intent unnecessarily.
        //  isBroadcastWorthy 这里返回true，只要是public就返回true
        if (mBootCompleted && isBroadcastWorthy(vol)) {
            final Intent intent = new Intent(VolumeInfo.ACTION_VOLUME_STATE_CHANGED);
            intent.putExtra(VolumeInfo.EXTRA_VOLUME_ID, vol.id);
            intent.putExtra(VolumeInfo.EXTRA_VOLUME_STATE, newState);
            intent.putExtra(VolumeRecord.EXTRA_FS_UUID, vol.fsUuid);
            intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
            //这里mHandler直接发送一个intent广播
            mHandler.obtainMessage(H_INTERNAL_BROADCAST, intent).sendToTarget();
        }

        final String oldStateEnv = VolumeInfo.getEnvironmentForState(oldState);
        final String newStateEnv = VolumeInfo.getEnvironmentForState(newState);

        if (!Objects.equals(oldStateEnv, newStateEnv)) {
            // Kick state changed event towards all started users. Any users
            // started after this point will trigger additional
            // user-specific broadcasts.
            for (int userId : mSystemUnlockedUsers) {
                if (vol.isVisibleForRead(userId)) {
                    final StorageVolume userVol = vol.buildStorageVolume(mContext, userId, false);
                    mHandler.obtainMessage(H_VOLUME_BROADCAST, userVol).sendToTarget();

                    mCallbacks.notifyStorageStateChanged(userVol.getPath(), oldStateEnv, newStateEnv);
                }
            }
        }

        if (vol.type == VolumeInfo.TYPE_PUBLIC && vol.state == VolumeInfo.STATE_EJECTING) {
            // TODO: this should eventually be handled by new ObbVolume state changes
            /*
             * Some OBBs might have been unmounted when this volume was
             * unmounted, so send a message to the handler to let it know to
             * remove those from the list of mounted OBBS.
             */
            mObbActionHandler.sendMessage(mObbActionHandler.obtainMessage(OBB_FLUSH_MOUNT_STATE, vol.path));
        }
    }



    private void onVolumeCreatedLocked(VolumeInfo vol) {
        if (mPms.isOnlyCoreApps()) {
            Slog.d(TAG, "System booted in core-only mode; ignoring volume " + vol.getId());
            return;
        }

        if (vol.type == VolumeInfo.TYPE_EMULATED) {
            final StorageManager storage = mContext.getSystemService(StorageManager.class);
            final VolumeInfo privateVol = storage.findPrivateForEmulated(vol);

            if (Objects.equals(StorageManager.UUID_PRIVATE_INTERNAL, mPrimaryStorageUuid)
                    && VolumeInfo.ID_PRIVATE_INTERNAL.equals(privateVol.id)) {
                Slog.v(TAG, "Found primary storage at " + vol);
                vol.mountFlags |= VolumeInfo.MOUNT_FLAG_PRIMARY;
                vol.mountFlags |= VolumeInfo.MOUNT_FLAG_VISIBLE;
                mHandler.obtainMessage(H_VOLUME_MOUNT, vol).sendToTarget();

            } else if (Objects.equals(privateVol.fsUuid, mPrimaryStorageUuid)) {
                Slog.v(TAG, "Found primary storage at " + vol);
                vol.mountFlags |= VolumeInfo.MOUNT_FLAG_PRIMARY;
                vol.mountFlags |= VolumeInfo.MOUNT_FLAG_VISIBLE;
                mHandler.obtainMessage(H_VOLUME_MOUNT, vol).sendToTarget();
            }

        } else if (vol.type == VolumeInfo.TYPE_PUBLIC) {
            // TODO: only look at first public partition
            //  vol.disk.isDefaultPrimary() == 2;
            if (Objects.equals(StorageManager.UUID_PRIMARY_PHYSICAL, mPrimaryStorageUuid) && vol.disk.isDefaultPrimary()) {
                Slog.v(TAG, "Found primary storage at " + vol);
                vol.mountFlags |= VolumeInfo.MOUNT_FLAG_PRIMARY;
                vol.mountFlags |= VolumeInfo.MOUNT_FLAG_VISIBLE;
            }

            // Adoptable public disks are visible to apps, since they meet
            // public API requirement of being in a stable location.
            if (vol.disk.isAdoptable()) {
                vol.mountFlags |= VolumeInfo.MOUNT_FLAG_VISIBLE;
            }

            vol.mountUserId = mCurrentUserId;
            mHandler.obtainMessage(H_VOLUME_MOUNT, vol).sendToTarget();

        } else if (vol.type == VolumeInfo.TYPE_PRIVATE) {
            mHandler.obtainMessage(H_VOLUME_MOUNT, vol).sendToTarget();

        } else {
            Slog.d(TAG, "Skipping automatic mounting of " + vol);
        }
    }


    /**
     * Decide if volume is mountable per device policies.
     */
    private boolean isMountDisallowed(VolumeInfo vol) {
        UserManager userManager = mContext.getSystemService(UserManager.class);

        boolean isUsbRestricted = false;
        if (vol.disk != null && vol.disk.isUsb()) {
            isUsbRestricted = userManager.hasUserRestriction(UserManager.DISALLOW_USB_FILE_TRANSFER,
                    Binder.getCallingUserHandle());
        }

        boolean isTypeRestricted = false;
        if (vol.type == VolumeInfo.TYPE_PUBLIC || vol.type == VolumeInfo.TYPE_PRIVATE) {
            isTypeRestricted = userManager
                    .hasUserRestriction(UserManager.DISALLOW_MOUNT_PHYSICAL_MEDIA,
                    Binder.getCallingUserHandle());
        }

        return isUsbRestricted || isTypeRestricted;
    }

    class StorageManagerServiceHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
     
            switch (msg.what) {
                case H_VOLUME_MOUNT: {
                    final VolumeInfo vol = (VolumeInfo) msg.obj;
                    if (isMountDisallowed(vol)) {
                        Slog.i(TAG, "Ignoring mount " + vol.getId() + " due to policy");
                        break;
                    }
                    try {
                        mConnector.execute("volume", "mount", vol.id, vol.mountFlags,vol.mountUserId);
                    } catch (NativeDaemonConnectorException ignored) {
                    }
                    break;
                }
            }
        }
    }

}






