

public class DdmRegister {

	public static void registerHandlers() {
        DdmHandleHello.register();
        DdmHandleThread.register();
        DdmHandleHeap.register();
        DdmHandleNativeHeap.register();
        DdmHandleProfiling.register();
        DdmHandleExit.register();
        DdmHandleViewDebug.register();

        DdmServer.registrationComplete();
    }

}