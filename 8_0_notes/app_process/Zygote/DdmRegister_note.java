/**
 * Just a place to stick handler registrations, instead of scattering
 * them around.
 */
public class DdmRegister {

    private DdmRegister() {}

    /**
     * Register handlers for all known chunk types.
     *
     * If you write a handler, add a registration call here.
     *
     * Note that this is invoked by the application (usually through a
     * static initializer in the main class), not the VM.  It's done this
     * way so that the handlers can use Android classes with native calls
     * that aren't registered until after the VM is initialized (e.g.
     * logging).  It also allows debugging of DDM handler initialization.
     *
     * The chunk dispatcher will pause until we call registrationComplete(),
     * so that we don't have a race that causes us to drop packets before
     * we finish here.
     */
    public static void registerHandlers() {
        if (false){
            Log.v("ddm", "Registering DDM message handlers");
        }
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