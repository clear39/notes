public class VerifierInfo implements Parcelable {
	   /** Package name of the verifier. */
    public final String packageName;

    /** Signatures used to sign the package verifier's package. */
    public final PublicKey publicKey;
}