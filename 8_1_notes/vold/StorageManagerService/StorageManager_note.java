public class StorageManager {

    /** {@hide}
     * Is this device already encrypted(加密)?
     * @return true for encrypted. (Implies isEncryptable() == true)
     *         false not encrypted
     */
    //  public static final String CRYPTO_STATE = SystemProperties.get("ro.crypto.state");  //这里值为 “encrypted”
    public static boolean isEncrypted() {
        return RoSystemProperties.CRYPTO_ENCRYPTED;//public static final boolean CRYPTO_ENCRYPTED = "encrypted".equalsIgnoreCase(CRYPTO_STATE);
    }

    /** {@hide}
     * Is this device file encrypted?
     * @return true for file encrypted. (Implies isEncrypted() == true)
     *         false not encrypted or block encrypted
     */
    public static boolean isFileEncryptedNativeOnly() {
        if (!isEncrypted()) {//isEncrypted返回true
            return false;
        }
        //public static final String CRYPTO_TYPE = SystemProperties.get("ro.crypto.type");  //当前值为“file”
        return RoSystemProperties.CRYPTO_FILE_ENCRYPTED;//  public static final boolean CRYPTO_FILE_ENCRYPTED = "file".equalsIgnoreCase(CRYPTO_TYPE);
    }
}