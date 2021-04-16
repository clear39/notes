


public class DdmServer {

	private static HashMap<Integer,ChunkHandler> mHandlerMap = new HashMap<Integer,ChunkHandler>();

	public static void registerHandler(int type, ChunkHandler handler) {
        if (handler == null) {
            throw new NullPointerException("handler == null");
        }
        synchronized (mHandlerMap) {
            if (mHandlerMap.get(type) != null)
                throw new RuntimeException("type " + Integer.toHexString(type) + " already registered");

            mHandlerMap.put(type, handler);
        }
    }
}