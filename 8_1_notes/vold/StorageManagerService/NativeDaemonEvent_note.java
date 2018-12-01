
/**
 * Parsed event from native side of {@link NativeDaemonConnector}.
 */
public class NativeDaemonEvent {

	/**
     * Test if event represents an unsolicited event from native daemon.
     */
    public boolean isClassUnsolicited() {
        return isClassUnsolicited(mCode);
    }

    private static boolean isClassUnsolicited(int code) {
        return code >= 600 && code < 700;
    }

	/**
     * Parse the given raw event into {@link NativeDaemonEvent} instance.
     *
     * @throws IllegalArgumentException when line doesn't match format expected
     *             from native side.
     */
    public static NativeDaemonEvent parseRawEvent(String rawEvent, FileDescriptor[] fdList) {
        final String[] parsed = rawEvent.split(" ");
        if (parsed.length < 2) {
            throw new IllegalArgumentException("Insufficient arguments");
        }

        int skiplength = 0;

        final int code;
        try {
            code = Integer.parseInt(parsed[0]);
            skiplength = parsed[0].length() + 1;
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException("problem parsing code", e);
        }

        int cmdNumber = -1;
        if (isClassUnsolicited(code) == false) {//code值不在600和700之间，执行一下代码
            if (parsed.length < 3) {
                throw new IllegalArgumentException("Insufficient arguemnts");
            }
            try {
                cmdNumber = Integer.parseInt(parsed[1]);
                skiplength += parsed[1].length() + 1;
            } catch (NumberFormatException e) {
                throw new IllegalArgumentException("problem parsing cmdNumber", e);
            }
        }

        String logMessage = rawEvent;
        if (parsed.length > 2 && parsed[2].equals(SENSITIVE_MARKER)) {//	static public final String SENSITIVE_MARKER = "{{sensitive}}";
            skiplength += parsed[2].length() + 1;
            logMessage = parsed[0] + " " + parsed[1] + " {}";
        }

        final String message = rawEvent.substring(skiplength);

        return new NativeDaemonEvent(cmdNumber, code, message, rawEvent, logMessage, fdList);
    }

    private NativeDaemonEvent(int cmdNumber, int code, String message,String rawEvent, String logMessage, FileDescriptor[] fdList) {
        mCmdNumber = cmdNumber;
        mCode = code;
        mMessage = message;
        mRawEvent = rawEvent;
        mLogMessage = logMessage;
        mParsed = null;
        mFdList = fdList;
    }

}