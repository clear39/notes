Installer installer = mSystemServiceManager.startService(Installer.class);


//	@frameworks/base/services/core/java/com/android/server/SystemServiceManager.java
mSystemServiceManager = new SystemServiceManager(mSystemContext);
mSystemServiceManager.setRuntimeRestarted(mRuntimeRestart);


public class SystemServiceManager {
	SystemServiceManager(Context context) {
        mContext = context;
    }


    void setRuntimeRestarted(boolean runtimeRestarted) {
        mRuntimeRestarted = runtimeRestarted;//	mRuntimeRestart = "1".equals(SystemProperties.get("sys.boot_completed"));
    }


    public SystemService startService(String className) {
        final Class<SystemService> serviceClass;
        try {
            serviceClass = (Class<SystemService>)Class.forName(className);
        } catch (ClassNotFoundException ex) {
            Slog.i(TAG, "Starting " + className);
            throw new RuntimeException("Failed to create service " + className
                    + ": service class not found, usually indicates that the caller should "
                    + "have called PackageManager.hasSystemFeature() to check whether the "
                    + "feature is available on this device before trying to start the "
                    + "services that implement it", ex);
        }
        return startService(serviceClass);
    }


    public <T extends SystemService> T startService(Class<T> serviceClass) {
	    try {
	        final String name = serviceClass.getName();
	        Slog.i(TAG, "Starting " + name);
	        Trace.traceBegin(Trace.TRACE_TAG_SYSTEM_SERVER, "StartService " + name);

	        // Create the service.
	        if (!SystemService.class.isAssignableFrom(serviceClass)) {
	            throw new RuntimeException("Failed to create " + name + ": service must extend " + SystemService.class.getName());
	        }
	        final T service;
	        try {
	            Constructor<T> constructor = serviceClass.getConstructor(Context.class);
	            service = constructor.newInstance(mContext);
	        } catch (InstantiationException ex) {
	            throw new RuntimeException("Failed to create service " + name
	                    + ": service could not be instantiated", ex);
	        } catch (IllegalAccessException ex) {
	            throw new RuntimeException("Failed to create service " + name
	                    + ": service must have a public constructor with a Context argument", ex);
	        } catch (NoSuchMethodException ex) {
	            throw new RuntimeException("Failed to create service " + name
	                    + ": service must have a public constructor with a Context argument", ex);
	        } catch (InvocationTargetException ex) {
	            throw new RuntimeException("Failed to create service " + name
	                    + ": service constructor threw an exception", ex);
	        }

	        startService(service);
	        return service;
	    } finally {
	        Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
	    }
    }


    public void startService(@NonNull final SystemService service) {
        // Register it.
        mServices.add(service);	//private final ArrayList<SystemService> mServices = new ArrayList<SystemService>();
        // Start it.
        long time = SystemClock.elapsedRealtime();
        try {
            service.onStart();
        } catch (RuntimeException ex) {
            throw new RuntimeException("Failed to start service " + service.getClass().getName()
                    + ": onStart threw an exception", ex);
        }
        warnIfTooLong(SystemClock.elapsedRealtime() - time, service, "onStart");
    }


}


//	@/frameworks/base/services/core/java/com/android/server/SystemService.java
public abstract class SystemService {
	public SystemService(Context context) {
        mContext = context;
    }
}


//	@frameworks/base/services/core/java/com/android/server/pm/Installer.java
public class Installer extends SystemService {
	public Installer(Context context) {
        this(context, false);
    }

    public Installer(Context context, boolean isolated = false) {
        super(context);
        mIsolated = isolated;
    }


    public void onStart() {
        if (mIsolated) { //	mIsolated = false;
            mInstalld = null;
        } else {
            connect();
        }
    }

    private void connect() {
        IBinder binder = ServiceManager.getService("installd");
        if (binder != null) {
            try {
                binder.linkToDeath(new DeathRecipient() {
                    @Override
                    public void binderDied() {
                        Slog.w(TAG, "installd died; reconnecting");
                        connect();
                    }
                }, 0);
            } catch (RemoteException e) {
                binder = null;
            }
        }

        if (binder != null) {
            mInstalld = IInstalld.Stub.asInterface(binder);
            try {
                invalidateMounts();
            } catch (InstallerException ignored) {
            }
        } else {
            Slog.w(TAG, "installd not found; trying again");
            BackgroundThread.getHandler().postDelayed(() -> {
                connect();
            }, DateUtils.SECOND_IN_MILLIS);
        }
    }






    

}	

