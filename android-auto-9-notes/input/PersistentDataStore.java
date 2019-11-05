
/***
 * 
 */
final class PersistentDataStore {

    public PersistentDataStore() {
        mAtomicFile = new AtomicFile(new File("/data/system/input-manager-state.xml"),"input-state");
    }

    @Nullable
    public String getCurrentKeyboardLayout() {
        return mCurrentKeyboardLayout;
    }
}