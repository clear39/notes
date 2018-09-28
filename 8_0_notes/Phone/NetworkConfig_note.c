/**
 * Describes the buildtime configuration of a network.
 * Holds settings read from resources.
 * @hide
 */
public class NetworkConfig {
    /**
     * Human readable string
     */
    public String name;

    /**
     * Type from ConnectivityManager
     */
    public int type;

    /**
     * the radio number from radio attributes config
     */
    public int radio;

    /**
     * higher number == higher priority when turning off connections
     */
    public int priority;

    /**
     * indicates the boot time dependencyMet setting
     */
    public boolean dependencyMet;

    /**
     * indicates the default restoral timer in seconds
     * if the network is used as a special network feature
     * -1 indicates no restoration of default
     */
    public int restoreTime;

    /**
     * input string from config.xml resource.  Uses the form:
     * [Connection name],[ConnectivityManager connection type],
     * [associated radio-type],[priority],[dependencyMet]
     */
    public NetworkConfig(String init) {
        String fragments[] = init.split(",");
        name = fragments[0].trim().toLowerCase(Locale.ROOT);
        type = Integer.parseInt(fragments[1]);
        radio = Integer.parseInt(fragments[2]);
        priority = Integer.parseInt(fragments[3]);
        restoreTime = Integer.parseInt(fragments[4]);
        dependencyMet = Boolean.parseBoolean(fragments[5]);
    }