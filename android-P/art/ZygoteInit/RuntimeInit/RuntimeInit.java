


public class RuntimeInit {

	static final void enableDdms() {
        // Register handlers for DDM messages.
        android.ddm.DdmRegister.registerHandlers();
    }
    

}