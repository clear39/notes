public class PhoneFactory {

    public static void makeDefaultPhone(Context context) {
        synchronized (sLockProxyPhones) {
            if (!sMadeDefaults) {
                sContext = context;



                /**
                 * 读取下面数组
                 * <string-array translatable="false" name="config_telephonyHardware">                                                                                                                                  
                 *  <!-- modem -->
                 *  <item>0,modem,0,0,0,1,1,1</item>
                 *  <!-- sim -->
                 *  <item>1,sim,0,modem</item>
                 * </string-array>
                 */

                // create the telephony device controller.
                TelephonyDevController.create();

                int retryCount = 0;
                for(;;) {
                    boolean hasException = false;
                    retryCount ++;

                    try {
                        // use UNIX domain socket to
                        // prevent subsequent initialization
                        new LocalServerSocket("com.android.internal.telephony");
                    } catch (java.io.IOException ex) {
                        hasException = true;
                    }

                    if ( !hasException ) {
                        break;
                    } else if (retryCount > SOCKET_OPEN_MAX_RETRY) {
                        throw new RuntimeException("PhoneFactory probably already running");
                    } else {
                        try {
                            Thread.sleep(SOCKET_OPEN_RETRY_MILLIS);
                        } catch (InterruptedException er) {
                        }
                    }
                }

                /**
                 * DefaultPhoneNotifier 封装了telephony.registry 服务调用接口
                 */
                sPhoneNotifier = new DefaultPhoneNotifier();

                int cdmaSubscription = CdmaSubscriptionSourceManager.getDefault(context);
                Rlog.i(LOG_TAG, "Cdma Subscription set to " + cdmaSubscription);



                 /**
                 * 获取 static final String PROPERTY_MULTI_SIM_CONFIG = "persist.radio.multisim.config"; 属性值，进行匹配支持几张sim卡
                 */
                /* In case of multi SIM mode two instances of Phone, RIL are created,
                   where as in single SIM mode only instance. isMultiSimEnabled() function checks
                   whether it is single SIM or multi SIM mode */
                int numPhones = TelephonyManager.getDefault().getPhoneCount();



                /**
                 * frameworks/base/core/res/res/values/config.xml:2702:    <bool name="config_dynamic_bind_ims">false</bool>
                 */
                // Return whether or not the device should use dynamic binding or the static
                // implementation (deprecated)
                boolean isDynamicBinding = sContext.getResources().getBoolean(
                        com.android.internal.R.bool.config_dynamic_bind_ims);

                /**
                 * frameworks/base/core/res/res/values/config.xml:2699:    <string name="config_ims_package"/>
                 * 这里是空的
                 */
                // Get the package name of the default IMS implementation.
                String defaultImsPackage = sContext.getResources().getString(
                        com.android.internal.R.string.config_ims_package);
                // Start ImsResolver and bind to ImsServices.
                Rlog.i(LOG_TAG, "ImsResolver: defaultImsPackage: " + defaultImsPackage);
                sImsResolver = new ImsResolver(sContext, defaultImsPackage, numPhones,isDynamicBinding);
                sImsResolver.initPopulateCacheAndStartBind();

                int[] networkModes = new int[numPhones];
                sPhones = new Phone[numPhones];
                sCommandsInterfaces = new RIL[numPhones];
                sTelephonyNetworkFactories = new TelephonyNetworkFactory[numPhones];

                for (int i = 0; i < numPhones; i++) {
                    // reads the system properties and makes commandsinterface
                    // Get preferred network type.
                    networkModes[i] = RILConstants.PREFERRED_NETWORK_MODE;

                    Rlog.i(LOG_TAG, "Network Mode set to " + Integer.toString(networkModes[i]));
                    sCommandsInterfaces[i] = new RIL(context, networkModes[i],cdmaSubscription, i);
                }
                Rlog.i(LOG_TAG, "Creating SubscriptionController");
                SubscriptionController.init(context, sCommandsInterfaces);

                // Instantiate UiccController so that all other classes can just
                // call getInstance()
                sUiccController = UiccController.make(context, sCommandsInterfaces);

                if (context.getPackageManager().hasSystemFeature(
                        PackageManager.FEATURE_TELEPHONY_EUICC)) {
                    sEuiccController = EuiccController.init(context);
                    sEuiccCardController = EuiccCardController.init(context);
                }

                for (int i = 0; i < numPhones; i++) {
                    Phone phone = null;
                    int phoneType = TelephonyManager.getPhoneType(networkModes[i]);
                    if (phoneType == PhoneConstants.PHONE_TYPE_GSM) {
                        phone = new GsmCdmaPhone(context,
                                sCommandsInterfaces[i], sPhoneNotifier, i,
                                PhoneConstants.PHONE_TYPE_GSM,
                                TelephonyComponentFactory.getInstance());
                    } else if (phoneType == PhoneConstants.PHONE_TYPE_CDMA) {
                        phone = new GsmCdmaPhone(context,
                                sCommandsInterfaces[i], sPhoneNotifier, i,
                                PhoneConstants.PHONE_TYPE_CDMA_LTE,
                                TelephonyComponentFactory.getInstance());
                    }
                    Rlog.i(LOG_TAG, "Creating Phone with type = " + phoneType + " sub = " + i);

                    sPhones[i] = phone;
                }

                // Set the default phone in base class.
                // FIXME: This is a first best guess at what the defaults will be. It
                // FIXME: needs to be done in a more controlled manner in the future.
                sPhone = sPhones[0];
                sCommandsInterface = sCommandsInterfaces[0];

                // Ensure that we have a default SMS app. Requesting the app with
                // updateIfNeeded set to true is enough to configure a default SMS app.
                ComponentName componentName =
                        SmsApplication.getDefaultSmsApplication(context, true /* updateIfNeeded */);
                String packageName = "NONE";
                if (componentName != null) {
                    packageName = componentName.getPackageName();
                }
                Rlog.i(LOG_TAG, "defaultSmsApplication: " + packageName);

                // Set up monitor to watch for changes to SMS packages
                SmsApplication.initSmsPackageMonitor(context);

                sMadeDefaults = true;

                Rlog.i(LOG_TAG, "Creating SubInfoRecordUpdater ");
                sSubInfoRecordUpdater = new SubscriptionInfoUpdater(
                        BackgroundThread.get().getLooper(), context, sPhones, sCommandsInterfaces);
                SubscriptionController.getInstance().updatePhonesAvailability(sPhones);

                // Start monitoring after defaults have been made.
                // Default phone must be ready before ImsPhone is created because ImsService might
                // need it when it is being opened. This should initialize multiple ImsPhones for
                // ImsResolver implementations of ImsService.
                for (int i = 0; i < numPhones; i++) {
                    sPhones[i].startMonitoringImsService();
                }

                ITelephonyRegistry tr = ITelephonyRegistry.Stub.asInterface(
                        ServiceManager.getService("telephony.registry"));
                SubscriptionController sc = SubscriptionController.getInstance();

                sSubscriptionMonitor = new SubscriptionMonitor(tr, sContext, sc, numPhones);

                sPhoneSwitcher = new PhoneSwitcher(MAX_ACTIVE_PHONES, numPhones,
                        sContext, sc, Looper.myLooper(), tr, sCommandsInterfaces,
                        sPhones);

                sProxyController = ProxyController.getInstance(context, sPhones,
                        sUiccController, sCommandsInterfaces, sPhoneSwitcher);

                sIntentBroadcaster = IntentBroadcaster.getInstance(context);

                sNotificationChannelController = new NotificationChannelController(context);

                sTelephonyNetworkFactories = new TelephonyNetworkFactory[numPhones];
                for (int i = 0; i < numPhones; i++) {
                    sTelephonyNetworkFactories[i] = new TelephonyNetworkFactory(
                            sPhoneSwitcher, sc, sSubscriptionMonitor, Looper.myLooper(),
                            sContext, i, sPhones[i].mDcTracker);
                }
            }
        }
    }
}