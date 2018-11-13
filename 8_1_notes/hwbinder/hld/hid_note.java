//@	frameworks/base/cmds/hid/src/com/android/commands/hid/Hid.java
public class Hid {
	public static void main(String[] args) {
        if (args.length != 1) {
            usage();
            System.exit(1);
        }

        InputStream stream = null;
        try {
            if (args[0].equals("-")) {
                stream = System.in;
            } else {
                File f = new File(args[0]);
                stream = new FileInputStream(f);
            }
            (new Hid(stream)).run();
        } catch (Exception e) {
            error("HID injection failed.", e);
            System.exit(1);
        } finally {
            IoUtils.closeQuietly(stream);
        }
    }
}