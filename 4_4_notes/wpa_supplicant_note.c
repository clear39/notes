//	external/wpa_supplicant_8/wpa_supplicant/wpa_supplicant_i.h
/**
 * struct wpa_interface - Parameters for wpa_supplicant_add_iface()
 */
struct wpa_interface {
	/**
	 * confname - Configuration name (file or profile) name
	 *
	 * This can also be %NULL when a configuration file is not used. In
	 * that case, ctrl_interface must be set to allow the interface to be
	 * configured.
	 */
	const char *confname;

	/**
	 * confanother - Additional configuration name (file or profile) name
	 *
	 * This can also be %NULL when the additional configuration file is not
	 * used.
	 */
	const char *confanother;

	/**
	 * ctrl_interface - Control interface parameter
	 *
	 * If a configuration file is not used, this variable can be used to
	 * set the ctrl_interface parameter that would have otherwise been read
	 * from the configuration file. If both confname and ctrl_interface are
	 * set, ctrl_interface is used to override the value from configuration
	 * file.
	 */
	const char *ctrl_interface;

	/**
	 * driver - Driver interface name, or %NULL to use the default driver
	 */
	const char *driver;

	/**
	 * driver_param - Driver interface parameters
	 *
	 * If a configuration file is not used, this variable can be used to
	 * set the driver_param parameters that would have otherwise been read
	 * from the configuration file. If both confname and driver_param are
	 * set, driver_param is used to override the value from configuration
	 * file.
	 */
	const char *driver_param;

	/**
	 * ifname - Interface name
	 */
	const char *ifname;

	/**
	 * bridge_ifname - Optional bridge interface name
	 *
	 * If the driver interface (ifname) is included in a Linux bridge
	 * device, the bridge interface may need to be used for receiving EAPOL
	 * frames. This can be enabled by setting this variable to enable
	 * receiving of EAPOL frames from an additional interface.
	 */
	const char *bridge_ifname;

	/**
	 * p2p_mgmt - Interface used for P2P management (P2P Device operations)
	 *
	 * Indicates whether wpas_p2p_init() must be called for this interface.
	 * This is used only when the driver supports a dedicated P2P Device
	 * interface that is not a network interface.
	 */
	int p2p_mgmt;
};

/**
 * struct wpa_params - Parameters for wpa_supplicant_init()
 */
struct wpa_params {
	/**
	 * daemonize - Run %wpa_supplicant in the background
	 */
	int daemonize;

	/**
	 * wait_for_monitor - Wait for a monitor program before starting
	 */
	int wait_for_monitor;

	/**
	 * pid_file - Path to a PID (process ID) file
	 *
	 * If this and daemonize are set, process ID of the background process
	 * will be written to the specified file.
	 */
	char *pid_file;

	/**
	 * wpa_debug_level - Debugging verbosity level (e.g., MSG_INFO)
	 */
	int wpa_debug_level;

	/**
	 * wpa_debug_show_keys - Whether keying material is included in debug
	 *
	 * This parameter can be used to allow keying material to be included
	 * in debug messages. This is a security risk and this option should
	 * not be enabled in normal configuration. If needed during
	 * development or while troubleshooting, this option can provide more
	 * details for figuring out what is happening.
	 */
	int wpa_debug_show_keys;

	/**
	 * wpa_debug_timestamp - Whether to include timestamp in debug messages
	 */
	int wpa_debug_timestamp;

	/**
	 * ctrl_interface - Global ctrl_iface path/parameter
	 */
	char *ctrl_interface;

	/**
	 * ctrl_interface_group - Global ctrl_iface group
	 */
	char *ctrl_interface_group;

	/**
	 * dbus_ctrl_interface - Enable the DBus control interface
	 */
	int dbus_ctrl_interface;

	/**
	 * wpa_debug_file_path - Path of debug file or %NULL to use stdout
	 */
	const char *wpa_debug_file_path;

	/**
	 * wpa_debug_syslog - Enable log output through syslog
	 */
	int wpa_debug_syslog;

	/**
	 * wpa_debug_tracing - Enable log output through Linux tracing
	 */
	int wpa_debug_tracing;

	/**
	 * override_driver - Optional driver parameter override
	 *
	 * This parameter can be used to override the driver parameter in
	 * dynamic interface addition to force a specific driver wrapper to be
	 * used instead.
	 */
	char *override_driver;

	/**
	 * override_ctrl_interface - Optional ctrl_interface override
	 *
	 * This parameter can be used to override the ctrl_interface parameter
	 * in dynamic interface addition to force a control interface to be
	 * created.
	 */
	char *override_ctrl_interface;

	/**
	 * entropy_file - Optional entropy file
	 *
	 * This parameter can be used to configure wpa_supplicant to maintain
	 * its internal entropy store over restarts.
	 */
	char *entropy_file;
};


struct wpa_supplicant {
	struct wpa_global *global;
	struct wpa_supplicant *parent;
	struct wpa_supplicant *next;
	struct l2_packet_data *l2;
	struct l2_packet_data *l2_br;
	unsigned char own_addr[ETH_ALEN];
	char ifname[100];
#ifdef CONFIG_CTRL_IFACE_DBUS
	char *dbus_path;
#endif /* CONFIG_CTRL_IFACE_DBUS */
#ifdef CONFIG_CTRL_IFACE_DBUS_NEW
	char *dbus_new_path;
	char *dbus_groupobj_path;
#ifdef CONFIG_AP
	char *preq_notify_peer;
#endif /* CONFIG_AP */
#endif /* CONFIG_CTRL_IFACE_DBUS_NEW */
	char bridge_ifname[16];

	char *confname;
	char *confanother;
	struct wpa_config *conf;
	int countermeasures;
	os_time_t last_michael_mic_error;
	u8 bssid[ETH_ALEN];
	u8 pending_bssid[ETH_ALEN]; /* If wpa_state == WPA_ASSOCIATING, this
				     * field contains the target BSSID. */
	int reassociate; /* reassociation requested */
	int disconnected; /* all connections disabled; i.e., do no reassociate
			   * before this has been cleared */
	struct wpa_ssid *current_ssid;
	struct wpa_bss *current_bss;
	int ap_ies_from_associnfo;
	unsigned int assoc_freq;

	/* Selected configuration (based on Beacon/ProbeResp WPA IE) */
	int pairwise_cipher;
	int group_cipher;
	int key_mgmt;
	int wpa_proto;
	int mgmt_group_cipher;

	void *drv_priv; /* private data used by driver_ops */
	void *global_drv_priv;

	u8 *bssid_filter;
	size_t bssid_filter_count;

	u8 *disallow_aps_bssid;
	size_t disallow_aps_bssid_count;
	struct wpa_ssid_value *disallow_aps_ssid;
	size_t disallow_aps_ssid_count;

	enum { WPA_SETBAND_AUTO, WPA_SETBAND_5G, WPA_SETBAND_2G } setband;

	/* previous scan was wildcard when interleaving between
	 * wildcard scans and specific SSID scan when max_ssids=1 */
	int prev_scan_wildcard;
	struct wpa_ssid *prev_scan_ssid; /* previously scanned SSID;
					  * NULL = not yet initialized (start
					  * with wildcard SSID)
					  * WILDCARD_SSID_SCAN = wildcard
					  * SSID was used in the previous scan
					  */
#define WILDCARD_SSID_SCAN ((struct wpa_ssid *) 1)

	struct wpa_ssid *prev_sched_ssid; /* last SSID used in sched scan */
	int sched_scan_timeout;
	int sched_scan_interval;
	int first_sched_scan;
	int sched_scan_timed_out;

	void (*scan_res_handler)(struct wpa_supplicant *wpa_s,
				 struct wpa_scan_results *scan_res);
	struct dl_list bss; /* struct wpa_bss::list */
	struct dl_list bss_id; /* struct wpa_bss::list_id */
	size_t num_bss;
	unsigned int bss_update_idx;
	unsigned int bss_next_id;

	 /*
	  * Pointers to BSS entries in the order they were in the last scan
	  * results.
	  */
	struct wpa_bss **last_scan_res;
	unsigned int last_scan_res_used;
	unsigned int last_scan_res_size;
	int last_scan_full;
	struct os_time last_scan;

	struct wpa_driver_ops *driver;
	int interface_removed; /* whether the network interface has been
				* removed */
	struct wpa_sm *wpa;
	struct eapol_sm *eapol;

	struct ctrl_iface_priv *ctrl_iface;

	enum wpa_states wpa_state;
	int scanning;
	int sched_scanning;
	int new_connection;

	int eapol_received; /* number of EAPOL packets received after the
			     * previous association event */

	struct scard_data *scard;
	char imsi[20];
	int mnc_len;

	unsigned char last_eapol_src[ETH_ALEN];

	int keys_cleared;

	struct wpa_blacklist *blacklist;

	/**
	 * extra_blacklist_count - Sum of blacklist counts after last connection
	 *
	 * This variable is used to maintain a count of temporary blacklisting
	 * failures (maximum number for any BSS) over blacklist clear
	 * operations. This is needed for figuring out whether there has been
	 * failures prior to the last blacklist clear operation which happens
	 * whenever no other not-blacklisted BSS candidates are available. This
	 * gets cleared whenever a connection has been established successfully.
	 */
	int extra_blacklist_count;

	/**
	 * scan_req - Type of the scan request
	 */
	enum scan_req_type {
		/**
		 * NORMAL_SCAN_REQ - Normal scan request
		 *
		 * This is used for scans initiated by wpa_supplicant to find an
		 * AP for a connection.
		 */
		NORMAL_SCAN_REQ,

		/**
		 * INITIAL_SCAN_REQ - Initial scan request
		 *
		 * This is used for the first scan on an interface to force at
		 * least one scan to be run even if the configuration does not
		 * include any enabled networks.
		 */
		INITIAL_SCAN_REQ,

		/**
		 * MANUAL_SCAN_REQ - Manual scan request
		 *
		 * This is used for scans where the user request a scan or
		 * a specific wpa_supplicant operation (e.g., WPS) requires scan
		 * to be run.
		 */
		MANUAL_SCAN_REQ
	} scan_req;
	struct os_time scan_trigger_time;
	int scan_runs; /* number of scan runs since WPS was started */
	int *next_scan_freqs;
	int scan_interval; /* time in sec between scans to find suitable AP */
	int normal_scans; /* normal scans run before sched_scan */
	int scan_for_connection; /* whether the scan request was triggered for
				  * finding a connection */

	unsigned int drv_flags;
	unsigned int drv_enc;

	/*
	 * A bitmap of supported protocols for probe response offload. See
	 * struct wpa_driver_capa in driver.h
	 */
	unsigned int probe_resp_offloads;

	/* extended capabilities supported by the driver */
	const u8 *extended_capa, *extended_capa_mask;
	unsigned int extended_capa_len;

	int max_scan_ssids;
	int max_sched_scan_ssids;
	int sched_scan_supported;
	unsigned int max_match_sets;
	unsigned int max_remain_on_chan;
	unsigned int max_stations;

	int pending_mic_error_report;
	int pending_mic_error_pairwise;
	int mic_errors_seen; /* Michael MIC errors with the current PTK */

	struct wps_context *wps;
	int wps_success; /* WPS success event received */
	struct wps_er *wps_er;
	int blacklist_cleared;

	struct wpabuf *pending_eapol_rx;
	struct os_time pending_eapol_rx_time;
	u8 pending_eapol_rx_src[ETH_ALEN];
	unsigned int last_eapol_matches_bssid:1;

	struct ibss_rsn *ibss_rsn;

	int set_sta_uapsd;
	int sta_uapsd;
	int set_ap_uapsd;
	int ap_uapsd;

#ifdef CONFIG_SME
	struct {
		u8 ssid[32];
		size_t ssid_len;
		int freq;
		u8 assoc_req_ie[200];
		size_t assoc_req_ie_len;
		int mfp;
		int ft_used;
		u8 mobility_domain[2];
		u8 *ft_ies;
		size_t ft_ies_len;
		u8 prev_bssid[ETH_ALEN];
		int prev_bssid_set;
		int auth_alg;
		int proto;

		int sa_query_count; /* number of pending SA Query requests;
				     * 0 = no SA Query in progress */
		int sa_query_timed_out;
		u8 *sa_query_trans_id; /* buffer of WLAN_SA_QUERY_TR_ID_LEN *
					* sa_query_count octets of pending
					* SA Query transaction identifiers */
		struct os_time sa_query_start;
		u8 sched_obss_scan;
		u16 obss_scan_int;
		u16 bss_max_idle_period;
#ifdef CONFIG_SAE
		struct sae_data sae;
		struct wpabuf *sae_token;
		int sae_group_index;
#endif /* CONFIG_SAE */
	} sme;
#endif /* CONFIG_SME */

#ifdef CONFIG_AP
	struct hostapd_iface *ap_iface;
	void (*ap_configured_cb)(void *ctx, void *data);
	void *ap_configured_cb_ctx;
	void *ap_configured_cb_data;
#endif /* CONFIG_AP */

	unsigned int off_channel_freq;
	struct wpabuf *pending_action_tx;
	u8 pending_action_src[ETH_ALEN];
	u8 pending_action_dst[ETH_ALEN];
	u8 pending_action_bssid[ETH_ALEN];
	unsigned int pending_action_freq;
	int pending_action_no_cck;
	int pending_action_without_roc;
	void (*pending_action_tx_status_cb)(struct wpa_supplicant *wpa_s,
					    unsigned int freq, const u8 *dst,
					    const u8 *src, const u8 *bssid,
					    const u8 *data, size_t data_len,
					    enum offchannel_send_action_result
					    result);
	unsigned int roc_waiting_drv_freq;
	int action_tx_wait_time;

	int p2p_mgmt;

#ifdef CONFIG_P2P
	struct p2p_go_neg_results *go_params;
	int create_p2p_iface;
	u8 pending_interface_addr[ETH_ALEN];
	char pending_interface_name[100];
	int pending_interface_type;
	int p2p_group_idx;
	unsigned int pending_listen_freq;
	unsigned int pending_listen_duration;
	enum {
		NOT_P2P_GROUP_INTERFACE,
		P2P_GROUP_INTERFACE_PENDING,
		P2P_GROUP_INTERFACE_GO,
		P2P_GROUP_INTERFACE_CLIENT
	} p2p_group_interface;
	struct p2p_group *p2p_group;
	int p2p_long_listen; /* remaining time in long Listen state in ms */
	char p2p_pin[10];
	int p2p_wps_method;
	u8 p2p_auth_invite[ETH_ALEN];
	int p2p_sd_over_ctrl_iface;
	int p2p_in_provisioning;
	int pending_invite_ssid_id;
	int show_group_started;
	u8 go_dev_addr[ETH_ALEN];
	int pending_pd_before_join;
	u8 pending_join_iface_addr[ETH_ALEN];
	u8 pending_join_dev_addr[ETH_ALEN];
	int pending_join_wps_method;
	int p2p_join_scan_count;
	int auto_pd_scan_retry;
	int force_long_sd;
	u16 pending_pd_config_methods;
	enum {
		NORMAL_PD, AUTO_PD_GO_NEG, AUTO_PD_JOIN
	} pending_pd_use;

	/*
	 * Whether cross connection is disallowed by the AP to which this
	 * interface is associated (only valid if there is an association).
	 */
	int cross_connect_disallowed;

	/*
	 * Whether this P2P group is configured to use cross connection (only
	 * valid if this is P2P GO interface). The actual cross connect packet
	 * forwarding may not be configured depending on the uplink status.
	 */
	int cross_connect_enabled;

	/* Whether cross connection forwarding is in use at the moment. */
	int cross_connect_in_use;

	/*
	 * Uplink interface name for cross connection
	 */
	char cross_connect_uplink[100];

	unsigned int sta_scan_pending:1;
	unsigned int p2p_auto_join:1;
	unsigned int p2p_auto_pd:1;
	unsigned int p2p_persistent_group:1;
	unsigned int p2p_fallback_to_go_neg:1;
	unsigned int p2p_pd_before_go_neg:1;
	unsigned int p2p_go_ht40:1;
	unsigned int user_initiated_pd:1;
	int p2p_persistent_go_freq;
	int p2p_persistent_id;
	int p2p_go_intent;
	int p2p_connect_freq;
	struct os_time p2p_auto_started;
	struct wpa_ssid *p2p_last_4way_hs_fail;
#endif /* CONFIG_P2P */

	struct wpa_ssid *bgscan_ssid;
	const struct bgscan_ops *bgscan;
	void *bgscan_priv;

	const struct autoscan_ops *autoscan;
	struct wpa_driver_scan_params *autoscan_params;
	void *autoscan_priv;

	struct wpa_ssid *connect_without_scan;

	struct wps_ap_info *wps_ap;
	size_t num_wps_ap;
	int wps_ap_iter;

	int after_wps;
	int known_wps_freq;
	unsigned int wps_freq;
	u16 wps_ap_channel;
	int wps_fragment_size;
	int auto_reconnect_disabled;

	 /* Channel preferences for AP/P2P GO use */
	int best_24_freq;
	int best_5_freq;
	int best_overall_freq;

	struct gas_query *gas;

#ifdef CONFIG_INTERWORKING
	unsigned int fetch_anqp_in_progress:1;
	unsigned int network_select:1;
	unsigned int auto_select:1;
	unsigned int auto_network_select:1;
	unsigned int fetch_all_anqp:1;
	struct wpa_bss *interworking_gas_bss;
#endif /* CONFIG_INTERWORKING */
	unsigned int drv_capa_known;

	struct {
		struct hostapd_hw_modes *modes;
		u16 num_modes;
		u16 flags;
	} hw;

	int pno;

	/* WLAN_REASON_* reason codes. Negative if locally generated. */
	int disconnect_reason;

	struct ext_password_data *ext_pw;

	struct wpabuf *last_gas_resp;
	u8 last_gas_addr[ETH_ALEN];
	u8 last_gas_dialog_token;

	unsigned int no_keep_alive:1;

#ifdef CONFIG_WNM
	u8 wnm_dialog_token;
	u8 wnm_reply;
	u8 wnm_num_neighbor_report;
	u8 wnm_mode;
	u16 wnm_dissoc_timer;
	u8 wnm_validity_interval;
	u8 wnm_bss_termination_duration[12];
	struct neighbor_report *wnm_neighbor_report_elements;
#endif /* CONFIG_WNM */

#ifdef CONFIG_TESTING_GET_GTK
	u8 last_gtk[32];
	size_t last_gtk_len;
#endif /* CONFIG_TESTING_GET_GTK */

	unsigned int num_multichan_concurrent;
#ifdef REALTEK_WIFI_VENDOR
    u8 invited_peer[ETH_ALEN];
#endif
};


struct wpa_global {
	struct wpa_supplicant *ifaces;
	struct wpa_params params;
	struct ctrl_iface_global_priv *ctrl_iface;
	struct wpas_dbus_priv *dbus;
	void **drv_priv;
	size_t drv_count;
	struct os_time suspend_time;
	struct p2p_data *p2p;
	struct wpa_supplicant *p2p_init_wpa_s;
	struct wpa_supplicant *p2p_group_formation;
	struct wpa_supplicant *p2p_invite_group;
	u8 p2p_dev_addr[ETH_ALEN];
	struct os_time p2p_go_wait_client;
	struct dl_list p2p_srv_bonjour; /* struct p2p_srv_bonjour */
	struct dl_list p2p_srv_upnp; /* struct p2p_srv_upnp */
	int p2p_disabled;
	int cross_connection;
	struct wpa_freq_range *p2p_disallow_freq;
	unsigned int num_p2p_disallow_freq;
	enum wpa_conc_pref {
		WPA_CONC_PREF_NOT_SET,
		WPA_CONC_PREF_STA,
		WPA_CONC_PREF_P2P
	} conc_pref;
	unsigned int p2p_cb_on_scan_complete:1;
	unsigned int p2p_per_sta_psk:1;

#ifdef CONFIG_WIFI_DISPLAY
	int wifi_display;
#define MAX_WFD_SUBELEMS 10
	struct wpabuf *wfd_subelem[MAX_WFD_SUBELEMS];
#endif /* CONFIG_WIFI_DISPLAY */

	struct psk_list_entry *add_psk; /* From group formation */
};





//wpa_supplicant 入口 启动参数如下：
/*
/system/bin/wpa_supplicant
-iwlan0
-Dnl80211
-c/data/misc/wifi/wpa_supplicant.conf
-I/system/etc/wifi/wpa_supplicant_overlay.conf
-O/data/misc/wifi/sockets-pp2p_device=1
-e/data/misc/wifi/entropy.bin
-g@android:wpa_wlan0
*/
///	external/wpa_supplicant_8/wpa_supplicant/main.c
int main(int argc, char *argv[])
{
	int c, i;
	struct wpa_interface *ifaces, *iface;
	int iface_count, exitcode = -1;
	struct wpa_params params;
	struct wpa_global *global;

	//配置用户ID和组ID
	if (os_program_init())
		return -1;

	os_memset(&params, 0, sizeof(params));
	params.wpa_debug_level = MSG_INFO;

	iface = ifaces = os_zalloc(sizeof(struct wpa_interface));/////
	if (ifaces == NULL)
		return -1;
	iface_count = 1;

	wpa_supplicant_fd_workaround(1);//这里检测确保stdin，stdout,stderr没有被关闭

	for (;;) {
		c = getopt(argc, argv,"b:Bc:C:D:de:f:g:G:hi:I:KLNo:O:p:P:qsTtuvW");
		if (c < 0)
			break;
		switch (c) {
		case 'b':
			iface->bridge_ifname = optarg;
			break;
		case 'B':
			params.daemonize++;
			break;
		case 'c':
			iface->confname = optarg;//这里有效  /data/misc/wifi/wpa_supplicant.conf
			break;
		case 'C':
			iface->ctrl_interface = optarg;
			break;
		case 'D':
			iface->driver = optarg;//这里有效 nl80211
			break;
		case 'd':
#ifdef CONFIG_NO_STDOUT_DEBUG
			printf("Debugging disabled with "  "CONFIG_NO_STDOUT_DEBUG=y build time " "option.\n");
			goto out;
#else /* CONFIG_NO_STDOUT_DEBUG */
			params.wpa_debug_level--;
			break;
#endif /* CONFIG_NO_STDOUT_DEBUG */
		case 'e':
			params.entropy_file = optarg;//这里有效 /data/misc/wifi/entropy.bin
			break;
#ifdef CONFIG_DEBUG_FILE
		case 'f':
			params.wpa_debug_file_path = optarg;
			break;
#endif /* CONFIG_DEBUG_FILE */
		case 'g':
			params.ctrl_interface = optarg;//这里有效 @android:wpa_wlan0
			break;
		case 'G':
			params.ctrl_interface_group = optarg;
			break;
		case 'h':
			usage();
			exitcode = 0;
			goto out;
		case 'i':
			iface->ifname = optarg;//这里有效 wlan0
			break;
		case 'I':
			iface->confanother = optarg; //这里有效	/system/etc/wifi/wpa_supplicant_overlay.conf
			break;
		case 'K':
			params.wpa_debug_show_keys++;
			break;
		case 'L':
			license();
			exitcode = 0;
			goto out;
		case 'o':
			params.override_driver = optarg;
			break;
		case 'O':
			params.override_ctrl_interface = optarg;//这里有效  /data/misc/wifi/sockets-pp2p_device=1
			break;
		case 'p':
			iface->driver_param = optarg;
			break;
		case 'P':
			os_free(params.pid_file);
			params.pid_file = os_rel2abs_path(optarg);
			break;
		case 'q':
			params.wpa_debug_level++;
			break;
#ifdef CONFIG_DEBUG_SYSLOG
		case 's':
			params.wpa_debug_syslog++;
			break;
#endif /* CONFIG_DEBUG_SYSLOG */
#ifdef CONFIG_DEBUG_LINUX_TRACING
		case 'T':
			params.wpa_debug_tracing++;
			break;
#endif /* CONFIG_DEBUG_LINUX_TRACING */
		case 't':
			params.wpa_debug_timestamp++;
			break;
#ifdef CONFIG_DBUS
		case 'u':
			params.dbus_ctrl_interface = 1;
			break;
#endif /* CONFIG_DBUS */
		case 'v':
			printf("%s\n", wpa_supplicant_version);
			exitcode = 0;
			goto out;
		case 'W':
			params.wait_for_monitor++;
			break;
		case 'N':
			iface_count++;
			iface = os_realloc_array(ifaces, iface_count,sizeof(struct wpa_interface));
			if (iface == NULL)
				goto out;
			ifaces = iface;
			iface = &ifaces[iface_count - 1]; 
			os_memset(iface, 0, sizeof(*iface));
			break;
		default:
			usage();
			exitcode = 0;
			goto out;
		}
	}

	exitcode = 0;
	global = wpa_supplicant_init(&params);//	external/wpa_supplicant_8/wpa_supplicant/wpa_supplicant.c
	if (global == NULL) {
		wpa_printf(MSG_ERROR, "Failed to initialize wpa_supplicant");
		exitcode = -1;
		goto out;
	} else {
		wpa_printf(MSG_INFO, "Successfully initialized " "wpa_supplicant");
	}

	wpa_printf(MSG_DEBUG, "iface_count:%d",iface_count); // iface_count == 1

	for (i = 0; exitcode == 0 && i < iface_count; i++) {
		struct wpa_supplicant *wpa_s;

		 //ifaces[i].confname == /data/misc/wifi/wpa_supplicant.conf   ifaces[i].ifname == "wlan0"
		if ((ifaces[i].confname == NULL && ifaces[i].ctrl_interface == NULL) ||  ifaces[i].ifname == NULL) {
			if (iface_count == 1 && (params.ctrl_interface ||params.dbus_ctrl_interface)) // params.ctrl_interface = "@android:wpa_wlan0"
				break;//跳出循环

			usage();
			exitcode = -1;
			break;
		}
		wpa_s = wpa_supplicant_add_iface(global, &ifaces[i]);
		if (wpa_s == NULL) {
			exitcode = -1;
			break;
		}
#ifdef CONFIG_P2P   //这里定义了
		if (wpa_s->global->p2p == NULL && (wpa_s->drv_flags &WPA_DRIVER_FLAGS_DEDICATED_P2P_DEVICE) &&  wpas_p2p_add_p2pdev_interface(wpa_s) < 0)
			exitcode = -1;
#endif /* CONFIG_P2P */
	}

	if (exitcode == 0)
		exitcode = wpa_supplicant_run(global);

	wpa_supplicant_deinit(global);

out:
	wpa_supplicant_fd_workaround(0);
	os_free(ifaces);
	os_free(params.pid_file);

	os_program_deinit();

	return exitcode;
}




//////////////////////////////////////////////////////////////////////
int os_program_init(void)
{
#ifdef ANDROID
	/*
	 * We ignore errors here since errors are normal if we
	 * are already running as non-root.
	 */
#ifdef ANDROID_SETGROUPS_OVERRIDE
	gid_t groups[] = { ANDROID_SETGROUPS_OVERRIDE };
#else /* ANDROID_SETGROUPS_OVERRIDE */
	gid_t groups[] = { AID_INET, AID_WIFI, AID_KEYSTORE };
#endif /* ANDROID_SETGROUPS_OVERRIDE */
	struct __user_cap_header_struct header;
	struct __user_cap_data_struct cap;

	setgroups(sizeof(groups)/sizeof(groups[0]), groups);

	prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);

	setgid(AID_WIFI);
	setuid(AID_WIFI);

	header.version = _LINUX_CAPABILITY_VERSION;
	header.pid = 0;
	cap.effective = cap.permitted = (1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW);
	cap.inheritable = 0;
	capset(&header, &cap);
#endif /* ANDROID */

#ifdef WPA_TRACE //这里没有定义 
	dl_list_init(&alloc_list);
#endif /* WPA_TRACE */
	return 0;
}



///////////////////////////////////////////////////////////////////////////////////
//wpa_supplicant_fd_workaround(1);
static void wpa_supplicant_fd_workaround(int start)
{
#ifdef __linux__ 
	static int fd[3] = { -1, -1, -1 };
	int i;
	/* When started from pcmcia-cs scripts, wpa_supplicant might start with
	 * fd 0, 1, and 2 closed. This will cause some issues because many
	 * places in wpa_supplicant are still printing out to stdout. As a
	 * workaround, make sure that fd's 0, 1, and 2 are not used for other
	 * sockets. */
	if (start) {
		for (i = 0; i < 3; i++) {
			fd[i] = open("/dev/null", O_RDWR);
			if (fd[i] > 2) {
				close(fd[i]);
				fd[i] = -1;
				break;
			}
		}
	} else {
		for (i = 0; i < 3; i++) {
			if (fd[i] >= 0) {
				close(fd[i]);
				fd[i] = -1;
			}
		}
	}
#endif /* __linux__ */
}


//////////////////////////////////////////////////////////////////////////
//global = wpa_supplicant_init(&params);
struct wpa_global * wpa_supplicant_init(struct wpa_params *params)
{
	struct wpa_global *global;
	int ret, i;

	if (params == NULL)
		return NULL;

#ifdef CONFIG_DRIVER_NDIS //这里没有定义，跳过
	{
		void driver_ndis_init_ops(void);
		driver_ndis_init_ops();
	}
#endif /* CONFIG_DRIVER_NDIS */

#ifndef CONFIG_NO_WPA_MSG   //这里没有定义，需要执行
	//主要用来打印log信息获取wifi名称
	// wpa_msg_register_ifname_cb 		external/wpa_supplicant_8/wpa_supplicant/src/utils/wpa_debug.c
	// wpa_supplicant_msg_ifname_cb 	external/wpa_supplicant_8/wpa_supplicant/wpa_supplicant.c	
	wpa_msg_register_ifname_cb(wpa_supplicant_msg_ifname_cb); 
#endif /* CONFIG_NO_WPA_MSG */

	//注意这里params->wpa_debug_file_path 没有赋值
	// 这里需要打开CONFIG_DEBUG_FILE 且 params->wpa_debug_file_path 不为空 ，才会执行
	wpa_debug_open_file(params->wpa_debug_file_path);
	if (params->wpa_debug_syslog) //这里 params->wpa_debug_syslog 为 0
		wpa_debug_open_syslog();
	if (params->wpa_debug_tracing) { //这里 params->wpa_debug_tracing 为 0
		ret = wpa_debug_open_linux_tracing();
		if (ret) {
			wpa_printf(MSG_ERROR,"Failed to enable trace logging");
			return NULL;
		}
	}

	// 创建对应类型并且赋值初始化函数和处理函数，加入到列表中
	ret = eap_register_methods();//		external/wpa_supplicant_8/wpa_supplicant/eap_register.c
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to register EAP methods");
		if (ret == -2)
			wpa_printf(MSG_ERROR, "Two or more EAP methods used " "the same EAP type.");
		return NULL;
	}

	global = os_zalloc(sizeof(*global));
	if (global == NULL)
		return NULL;

	//global->p2p_srv_bonjour global->p2p_srv_upnp 初始化链表
	dl_list_init(&global->p2p_srv_bonjour);
	dl_list_init(&global->p2p_srv_upnp);
	global->params.daemonize = params->daemonize;  // params->daemonize == 0 
	global->params.wait_for_monitor = params->wait_for_monitor;	// params->wait_for_monitor = 0
	global->params.dbus_ctrl_interface = params->dbus_ctrl_interface;	// params->dbus_ctrl_interface = 0;

	if (params->pid_file)								//	params->pid_file == 0
		global->params.pid_file = os_strdup(params->pid_file);
	if (params->ctrl_interface)							//	params->ctrl_interface == @android:wpa_wlan0
		global->params.ctrl_interface = os_strdup(params->ctrl_interface);
	if (params->ctrl_interface_group)						//	params->ctrl_interface_group == 0
		global->params.ctrl_interface_group = os_strdup(params->ctrl_interface_group);
	if (params->override_driver)							//	params->override_driver == 0
		global->params.override_driver = os_strdup(params->override_driver);
	if (params->override_ctrl_interface)						//	params->override_ctrl_interface == /data/misc/wifi/sockets-pp2p_device=1
		global->params.override_ctrl_interface = os_strdup(params->override_ctrl_interface);



	wpa_debug_level = global->params.wpa_debug_level = params->wpa_debug_level;	//	params.wpa_debug_level = MSG_INFO;
	wpa_debug_show_keys = global->params.wpa_debug_show_keys = params->wpa_debug_show_keys;		// 0
	wpa_debug_timestamp = global->params.wpa_debug_timestamp = params->wpa_debug_timestamp;		// 0

	wpa_printf(MSG_DEBUG, "wpa_supplicant v" VERSION_STR);

	if (eloop_init()) {// 初始化全局变量eloop	//		external/wpa_supplicant_8/src/utils/eloop.c
		wpa_printf(MSG_ERROR, "Failed to initialize event loop");
		wpa_supplicant_deinit(global);
		return NULL;
	}

	random_init(params->entropy_file); // params->entropy_file == /data/misc/wifi/entropy.bin	//	external/wpa_supplicant_8/src/crypto/random.c 

	//打开wpa_wlan0的套接字，并且添加监听
	global->ctrl_iface = wpa_supplicant_global_ctrl_iface_init(global); //	external/wpa_supplicant_8/wpa_supplicant/ctrl_iface_unix.c
	if (global->ctrl_iface == NULL) {
		wpa_supplicant_deinit(global);
		return NULL;
	}

	//空实现
	if (wpas_notify_supplicant_initialized(global)) {//	external/wpa_supplicant_8/wpa_supplicant/notify.c
		wpa_supplicant_deinit(global);
		return NULL;
	}

	//	external/wpa_supplicant_8/src/drivers/drivers.c
	/***
	struct wpa_driver_ops *wpa_drivers[] =
	{
		#ifdef CONFIG_DRIVER_NL80211
			&wpa_driver_nl80211_ops,
		#endif /* CONFIG_DRIVER_NL80211 */
		#ifdef CONFIG_DRIVER_WEXT
			&wpa_driver_wext_ops,
		#endif /* CONFIG_DRIVER_WEXT */
		#ifdef CONFIG_DRIVER_HOSTAP
			&wpa_driver_hostap_ops,
		#endif /* CONFIG_DRIVER_HOSTAP */
		#ifdef CONFIG_DRIVER_MADWIFI
			&wpa_driver_madwifi_ops,
		#endif /* CONFIG_DRIVER_MADWIFI */
		#ifdef CONFIG_DRIVER_BSD
			&wpa_driver_bsd_ops,
		#endif /* CONFIG_DRIVER_BSD */
		#ifdef CONFIG_DRIVER_OPENBSD
			&wpa_driver_openbsd_ops,
		#endif /* CONFIG_DRIVER_OPENBSD */
		#ifdef CONFIG_DRIVER_NDIS
			&wpa_driver_ndis_ops,
		#endif /* CONFIG_DRIVER_NDIS */
		#ifdef CONFIG_DRIVER_WIRED
			&wpa_driver_wired_ops,
		#endif /* CONFIG_DRIVER_WIRED */
		#ifdef CONFIG_DRIVER_TEST
			&wpa_driver_test_ops,
		#endif /* CONFIG_DRIVER_TEST */
		#ifdef CONFIG_DRIVER_ROBOSWITCH
			&wpa_driver_roboswitch_ops,
		#endif /* CONFIG_DRIVER_ROBOSWITCH */
		#ifdef CONFIG_DRIVER_ATHEROS
			&wpa_driver_atheros_ops,
		#endif /* CONFIG_DRIVER_ATHEROS */
		#ifdef CONFIG_DRIVER_NONE
			&wpa_driver_none_ops,
		#endif /* CONFIG_DRIVER_NONE */
			NULL
	};
	*/
	for (i = 0; wpa_drivers[i]; i++)
		global->drv_count++;

	if (global->drv_count == 0) {
		wpa_printf(MSG_ERROR, "No drivers enabled");
		wpa_supplicant_deinit(global);
		return NULL;
	}
	global->drv_priv = os_zalloc(global->drv_count * sizeof(void *));
	if (global->drv_priv == NULL) {
		wpa_supplicant_deinit(global);
		return NULL;
	}

#ifdef CONFIG_WIFI_DISPLAY    //CONFIG_WIFI_DISPLAY 已经定义
	// 设置global->wifi_display = 1;	
	if (wifi_display_init(global) < 0) { //external/wpa_supplicant_8/wpa_supplicant/wifi_display.c
		wpa_printf(MSG_ERROR, "Failed to initialize Wi-Fi Display");
		wpa_supplicant_deinit(global);
		return NULL;
	}
#endif /* CONFIG_WIFI_DISPLAY */

	return global;
}

static wpa_msg_get_ifname_func wpa_msg_ifname_cb = NULL;
void wpa_msg_register_ifname_cb(wpa_msg_get_ifname_func func)
{
	wpa_msg_ifname_cb = func;
}

static const char * wpa_supplicant_msg_ifname_cb(void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	if (wpa_s == NULL)
		return NULL;
	return wpa_s->ifname;
}


int wpa_debug_open_file(const char *path)
{
#ifdef CONFIG_DEBUG_FILE  //这里默认没有定义，忽略
	if (!path)
		return 0;
    	wpa_printf(MSG_DEBUG,"%s %s",__func__,path);
	if (last_path == NULL || os_strcmp(last_path, path) != 0) {
		/* Save our path to enable re-open */
		os_free(last_path);
		last_path = os_strdup(path);
	}

	out_file = fopen(path, "a");
	if (out_file == NULL) {
		wpa_printf(MSG_ERROR, "wpa_debug_open_file: Failed to open " "output file, using standard output");
		return -1;
	}

#ifndef _WIN32  //这里忽略
	setvbuf(out_file, NULL, _IOLBF, 0);
#endif /* _WIN32 */

#endif /* CONFIG_DEBUG_FILE */
	return 0;
}



/**
 * eap_register_methods - Register statically linked EAP methods
 * Returns: 0 on success, -1 or -2 on failure
 *
 * This function is called at program initialization to register all EAP
 * methods that were linked in statically.
 */
int eap_register_methods(void)
{
	int ret = 0;

#ifdef EAP_MD5
	if (ret == 0)
		ret = eap_peer_md5_register(); //该宏定义，这里执行
#endif /* EAP_MD5 */

#ifdef EAP_TLS
	if (ret == 0)
		ret = eap_peer_tls_register(); //该宏定义，这里执行
#endif /* EAP_TLS */

#ifdef EAP_UNAUTH_TLS
	if (ret == 0)
		ret = eap_peer_unauth_tls_register(); 
#endif /* EAP_UNAUTH_TLS */

#ifdef EAP_MSCHAPv2
	if (ret == 0)
		ret = eap_peer_mschapv2_register();//该宏定义，这里执行
#endif /* EAP_MSCHAPv2 */

#ifdef EAP_PEAP
	if (ret == 0)
		ret = eap_peer_peap_register();//该宏定义，这里执行
#endif /* EAP_PEAP */

#ifdef EAP_TTLS
	if (ret == 0)
		ret = eap_peer_ttls_register();//该宏定义，这里执行
#endif /* EAP_TTLS */

#ifdef EAP_GTC
	if (ret == 0)
		ret = eap_peer_gtc_register();//该宏定义，这里执行
#endif /* EAP_GTC */

#ifdef EAP_OTP
	if (ret == 0)
		ret = eap_peer_otp_register();//该宏定义，这里执行
#endif /* EAP_OTP */

#ifdef EAP_SIM
	if (ret == 0)
		ret = eap_peer_sim_register();//该宏定义，这里执行
#endif /* EAP_SIM */

#ifdef EAP_LEAP
	if (ret == 0)
		ret = eap_peer_leap_register();//该宏定义，这里执行
#endif /* EAP_LEAP */

#ifdef EAP_PSK
	if (ret == 0)
		ret = eap_peer_psk_register();
#endif /* EAP_PSK */

#ifdef EAP_AKA
	if (ret == 0)
		ret = eap_peer_aka_register();//该宏定义，这里执行
#endif /* EAP_AKA */

#ifdef EAP_AKA_PRIME
	if (ret == 0)
		ret = eap_peer_aka_prime_register();
#endif /* EAP_AKA_PRIME */

#ifdef EAP_FAST
	if (ret == 0)
		ret = eap_peer_fast_register();//该宏定义，这里执行
#endif /* EAP_FAST */

#ifdef EAP_PAX
	if (ret == 0)
		ret = eap_peer_pax_register();
#endif /* EAP_PAX */

#ifdef EAP_SAKE
	if (ret == 0)
		ret = eap_peer_sake_register();
#endif /* EAP_SAKE */

#ifdef EAP_GPSK
	if (ret == 0)
		ret = eap_peer_gpsk_register();
#endif /* EAP_GPSK */

#ifdef EAP_WSC
	if (ret == 0)
		ret = eap_peer_wsc_register();
#endif /* EAP_WSC */

#ifdef EAP_IKEV2
	if (ret == 0)
		ret = eap_peer_ikev2_register();
#endif /* EAP_IKEV2 */

#ifdef EAP_VENDOR_TEST
	if (ret == 0)
		ret = eap_peer_vendor_test_register();
#endif /* EAP_VENDOR_TEST */

#ifdef EAP_TNC
	if (ret == 0)
		ret = eap_peer_tnc_register();
#endif /* EAP_TNC */

#ifdef EAP_PWD
	if (ret == 0)
		ret = eap_peer_pwd_register();//该宏定义，这里执行
#endif /* EAP_PWD */

#ifdef EAP_EKE
	if (ret == 0)
		ret = eap_peer_eke_register();
#endif /* EAP_EKE */

#ifdef EAP_SERVER_IDENTITY
	if (ret == 0)
		ret = eap_server_identity_register();
#endif /* EAP_SERVER_IDENTITY */

#ifdef EAP_SERVER_MD5
	if (ret == 0)
		ret = eap_server_md5_register();
#endif /* EAP_SERVER_MD5 */

#ifdef EAP_SERVER_TLS
	if (ret == 0)
		ret = eap_server_tls_register();
#endif /* EAP_SERVER_TLS */

#ifdef EAP_SERVER_UNAUTH_TLS
	if (ret == 0)
		ret = eap_server_unauth_tls_register();
#endif /* EAP_SERVER_UNAUTH_TLS */

#ifdef EAP_SERVER_MSCHAPV2
	if (ret == 0)
		ret = eap_server_mschapv2_register();
#endif /* EAP_SERVER_MSCHAPV2 */

#ifdef EAP_SERVER_PEAP
	if (ret == 0)
		ret = eap_server_peap_register();
#endif /* EAP_SERVER_PEAP */

#ifdef EAP_SERVER_TLV
	if (ret == 0)
		ret = eap_server_tlv_register();
#endif /* EAP_SERVER_TLV */

#ifdef EAP_SERVER_GTC
	if (ret == 0)
		ret = eap_server_gtc_register();
#endif /* EAP_SERVER_GTC */

#ifdef EAP_SERVER_TTLS
	if (ret == 0)
		ret = eap_server_ttls_register();
#endif /* EAP_SERVER_TTLS */

#ifdef EAP_SERVER_SIM
	if (ret == 0)
		ret = eap_server_sim_register();
#endif /* EAP_SERVER_SIM */

#ifdef EAP_SERVER_AKA
	if (ret == 0)
		ret = eap_server_aka_register();
#endif /* EAP_SERVER_AKA */

#ifdef EAP_SERVER_AKA_PRIME
	if (ret == 0)
		ret = eap_server_aka_prime_register();
#endif /* EAP_SERVER_AKA_PRIME */

#ifdef EAP_SERVER_PAX
	if (ret == 0)
		ret = eap_server_pax_register();
#endif /* EAP_SERVER_PAX */

#ifdef EAP_SERVER_PSK
	if (ret == 0)
		ret = eap_server_psk_register();
#endif /* EAP_SERVER_PSK */

#ifdef EAP_SERVER_SAKE
	if (ret == 0)
		ret = eap_server_sake_register();
#endif /* EAP_SERVER_SAKE */

#ifdef EAP_SERVER_GPSK
	if (ret == 0)
		ret = eap_server_gpsk_register();
#endif /* EAP_SERVER_GPSK */

#ifdef EAP_SERVER_VENDOR_TEST
	if (ret == 0)
		ret = eap_server_vendor_test_register();
#endif /* EAP_SERVER_VENDOR_TEST */

#ifdef EAP_SERVER_FAST
	if (ret == 0)
		ret = eap_server_fast_register();
#endif /* EAP_SERVER_FAST */

#ifdef EAP_SERVER_WSC
	if (ret == 0)
		ret = eap_server_wsc_register();
#endif /* EAP_SERVER_WSC */

#ifdef EAP_SERVER_IKEV2
	if (ret == 0)
		ret = eap_server_ikev2_register();
#endif /* EAP_SERVER_IKEV2 */

#ifdef EAP_SERVER_TNC
	if (ret == 0)
		ret = eap_server_tnc_register();
#endif /* EAP_SERVER_TNC */

#ifdef EAP_SERVER_PWD
	if (ret == 0)
		ret = eap_server_pwd_register();
#endif /* EAP_SERVER_PWD */

	return ret;
}


//这里之分析一个其他一样
int eap_peer_md5_register(void)
{
	struct eap_method *eap;
	int ret;

	eap = eap_peer_method_alloc(EAP_PEER_METHOD_INTERFACE_VERSION,EAP_VENDOR_IETF, EAP_TYPE_MD5, "MD5"); //创建eap_method，并且赋值
	if (eap == NULL)
		return -1;

	eap->init = eap_md5_init;
	eap->deinit = eap_md5_deinit;
	eap->process = eap_md5_process;

	ret = eap_peer_method_register(eap);//加入到列表
	if (ret)
		eap_peer_method_free(eap);
	return ret;
}

struct eloop_sock {
	int sock;
	void *eloop_data;
	void *user_data;
	eloop_sock_handler handler;
	WPA_TRACE_REF(eloop);
	WPA_TRACE_REF(user);
	WPA_TRACE_INFO
};

struct eloop_timeout {
	struct dl_list list;
	struct os_time time;
	void *eloop_data;
	void *user_data;
	eloop_timeout_handler handler;
	WPA_TRACE_REF(eloop);
	WPA_TRACE_REF(user);
	WPA_TRACE_INFO
};

struct eloop_signal {
	int sig;
	void *user_data;
	eloop_signal_handler handler;
	int signaled;
};

struct eloop_sock_table {
	int count;
	struct eloop_sock *table;
	int changed;
};

struct eloop_data {
	int max_sock;

	int count; /* sum of all table counts */

#ifdef CONFIG_ELOOP_POLL  //	CONFIG_ELOOP_POLL 没有定义
	int max_pollfd_map; /* number of pollfds_map currently allocated */
	int max_poll_fds; /* number of pollfds currently allocated */
	struct pollfd *pollfds;
	struct pollfd **pollfds_map;
#endif /* CONFIG_ELOOP_POLL */

	struct eloop_sock_table readers;
	struct eloop_sock_table writers;
	struct eloop_sock_table exceptions;

	struct dl_list timeout;

	int signal_count;
	struct eloop_signal *signals;
	int signaled;
	int pending_terminate;

	int terminate;
	int reader_table_changed;
};

int eloop_init(void)
{
	// static struct eloop_data eloop;		external/wpa_supplicant_8/src/utils/eloop.c
	os_memset(&eloop, 0, sizeof(eloop));
	dl_list_init(&eloop.timeout);
#ifdef WPA_TRACE	//WPA_TRACE没有定义
	signal(SIGSEGV, eloop_sigsegv_handler);
#endif /* WPA_TRACE */
	return 0;
}


//	random_init(params->entropy_file); // params->entropy_file == /data/misc/wifi/entropy.bin
void random_init(const char *entropy_file)
{
    wpa_printf(MSG_DEBUG,"%s %s",__func__,entropy_file);
    os_free(random_entropy_file);
	if (entropy_file)
		random_entropy_file = os_strdup(entropy_file);
	else
		random_entropy_file = NULL;
	random_read_entropy();

#ifdef __linux__
	if (random_fd >= 0)
		return;

	random_fd = open("/dev/random", O_RDONLY | O_NONBLOCK);
	if (random_fd < 0) {
#ifndef CONFIG_NO_STDOUT_DEBUG
		int error = errno;
		perror("open(/dev/random)");
		wpa_printf(MSG_ERROR, "random: Cannot open /dev/random: %s",strerror(error));
#endif /* CONFIG_NO_STDOUT_DEBUG */
		return;
	}
	wpa_printf(MSG_DEBUG, "random: Trying to read entropy from " "/dev/random");

	eloop_register_read_sock(random_fd, random_read_fd, NULL, NULL);
#endif /* __linux__ */

	random_write_entropy();
}


static void random_read_entropy(void)
{
	char *buf;
	size_t len;

	if (!random_entropy_file)
		return;

	//os_readfile    external/wpa_supplicant_8/src/utils/os_unix.c
	buf = os_readfile(random_entropy_file, &len); //读取/data/misc/wifi/entropy.bin	全部数据存入buf中	
	if (buf == NULL)
		return; /* entropy file not yet available */

	//#define RANDOM_ENTROPY_SIZE 20  			external/wpa_supplicant_8/src/crypto/random.c
	if (len != 1 + RANDOM_ENTROPY_SIZE) {
		wpa_printf(MSG_DEBUG, "random: Invalid entropy file %s", random_entropy_file);
		os_free(buf);
		return;
	}

	own_pool_ready = (u8) buf[0];  // static unsigned int own_pool_ready = 0;默认值

	random_add_randomness(buf + 1, RANDOM_ENTROPY_SIZE);	//	RANDOM_ENTROPY_SIZE=20
	random_entropy_file_read = 1;
	os_free(buf);

	wpa_printf(MSG_DEBUG, "%s random: Added entropy from %s " "(own_pool_ready=%u)",__func__,random_entropy_file, own_pool_ready);
}


void random_add_randomness(const void *buf, size_t len)
{
	struct os_time t;
	static unsigned int count = 0;

	count++;
	if (entropy > MIN_COLLECT_ENTROPY && (count & 0x3ff) != 0) {
		/*
		 * No need to add more entropy at this point, so save CPU and
		 * skip the update.
		 */
		return;
	}
	wpa_printf(MSG_EXCESSIVE, "Add randomness: count=%u entropy=%u",count, entropy);

	os_get_time(&t);
	wpa_hexdump_key(MSG_EXCESSIVE, "random pool",(const u8 *) pool, sizeof(pool));
	random_mix_pool(&t, sizeof(t));
	random_mix_pool(buf, len);
	wpa_hexdump_key(MSG_EXCESSIVE, "random pool",(const u8 *) pool, sizeof(pool));
	entropy++;
	total_collected++;
}


static u32 __ROL32(u32 x, u32 y)
{
	return (x << (y & 31)) | (x >> (32 - (y & 31)));
}

static void random_mix_pool(const void *buf, size_t len)
{
	static const u32 twist[8] = {
		0x00000000, 0x3b6e20c8, 0x76dc4190, 0x4db26158,
		0xedb88320, 0xd6d6a3e8, 0x9b64c2b0, 0xa00ae278
	};
	const u8 *pos = buf;
	u32 w;

	wpa_hexdump_key(MSG_EXCESSIVE, "random_mix_pool", buf, len);

	while (len--) {
		w = __ROL32(*pos++, input_rotate & 31);
		input_rotate += pool_pos ? 7 : 14;
		pool_pos = (pool_pos - 1) & POOL_WORDS_MASK;
		w ^= pool[pool_pos];
		w ^= pool[(pool_pos + POOL_TAP1) & POOL_WORDS_MASK];
		w ^= pool[(pool_pos + POOL_TAP2) & POOL_WORDS_MASK];
		w ^= pool[(pool_pos + POOL_TAP3) & POOL_WORDS_MASK];
		w ^= pool[(pool_pos + POOL_TAP4) & POOL_WORDS_MASK];
		w ^= pool[(pool_pos + POOL_TAP5) & POOL_WORDS_MASK];
		pool[pool_pos] = (w >> 3) ^ twist[w & 7];
       		// wpa_printf(MSG_DEBUG,"%s pool[%d]:%d",__func__,pool_pos,pool[pool_pos]);
	}
}


int eloop_register_read_sock(int sock, eloop_sock_handler handler,void *eloop_data, void *user_data)
{
	return eloop_register_sock(sock, EVENT_TYPE_READ, handler,eloop_data, user_data);
}

int eloop_register_sock(int sock, eloop_event_type type,eloop_sock_handler handler,void *eloop_data, void *user_data)
{
	struct eloop_sock_table *table;
	table = eloop_get_sock_table(type);
	return eloop_sock_table_add_sock(table, sock, handler,eloop_data, user_data);
}

static struct eloop_sock_table *eloop_get_sock_table(eloop_event_type type)
{
	switch (type) {
	case EVENT_TYPE_READ:
		return &eloop.readers;
	case EVENT_TYPE_WRITE:
		return &eloop.writers;
	case EVENT_TYPE_EXCEPTION:
		return &eloop.exceptions;
	}

	return NULL;
}

static int eloop_sock_table_add_sock(struct eloop_sock_table *table,
                                     int sock, eloop_sock_handler handler,
                                     void *eloop_data, void *user_data)
{
	struct eloop_sock *tmp;
	int new_max_sock;

	if (sock > eloop.max_sock)
		new_max_sock = sock;
	else
		new_max_sock = eloop.max_sock;

	if (table == NULL)
		return -1;

#ifdef CONFIG_ELOOP_POLL	//没有定义
	if (new_max_sock >= eloop.max_pollfd_map) {
		struct pollfd **nmap;
		nmap = os_realloc_array(eloop.pollfds_map, new_max_sock + 50,
					sizeof(struct pollfd *));
		if (nmap == NULL)
			return -1;

		eloop.max_pollfd_map = new_max_sock + 50;
		eloop.pollfds_map = nmap;
	}

	if (eloop.count + 1 > eloop.max_poll_fds) {
		struct pollfd *n;
		int nmax = eloop.count + 1 + 50;
		n = os_realloc_array(eloop.pollfds, nmax,
				     sizeof(struct pollfd));
		if (n == NULL)
			return -1;

		eloop.max_poll_fds = nmax;
		eloop.pollfds = n;
	}
#endif /* CONFIG_ELOOP_POLL */

	eloop_trace_sock_remove_ref(table); //由于WPA_TRACE没有定义，函数空实现
	tmp = os_realloc_array(table->table, table->count + 1,sizeof(struct eloop_sock)); //重新分配空间
	if (tmp == NULL)
		return -1;

	tmp[table->count].sock = sock;
	tmp[table->count].eloop_data = eloop_data;
	tmp[table->count].user_data = user_data;
	tmp[table->count].handler = handler;
	wpa_trace_record(&tmp[table->count]);//由于WPA_TRACE没有定义，函数空实现
	table->count++;
	table->table = tmp;
	eloop.max_sock = new_max_sock;
	eloop.count++;
	table->changed = 1;
	eloop_trace_sock_add_ref(table);//由于WPA_TRACE没有定义，函数空实现

	return 0;
}


//分析global->ctrl_iface = wpa_supplicant_global_ctrl_iface_init(global);
struct ctrl_iface_global_priv *wpa_supplicant_global_ctrl_iface_init(struct wpa_global *global)
{
	struct ctrl_iface_global_priv *priv;
	struct sockaddr_un addr;
	const char *ctrl = global->params.ctrl_interface; // global->params.ctrl_interface == @android:wpa_wlan0
	int flags;

	priv = os_zalloc(sizeof(*priv));
	if (priv == NULL)
		return NULL;
	dl_list_init(&priv->ctrl_dst);
	priv->global = global;
	priv->sock = -1;

	if (ctrl == NULL)
		return priv;

	wpa_printf(MSG_DEBUG, "Global control interface '%s'", ctrl);

#ifdef ANDROID
	if (os_strncmp(ctrl, "@android:", 9) == 0) {
		priv->sock = android_get_control_socket(ctrl + 9);// wpa_wlan0
		if (priv->sock < 0) {
			wpa_printf(MSG_ERROR, "Failed to open Android control " "socket '%s'", ctrl + 9);
			goto fail;
		}
		wpa_printf(MSG_DEBUG, "Using Android control socket '%s'", ctrl + 9);
		goto havesock; //跳转到最后
	}

//后面不执行
	if (os_strncmp(ctrl, "@abstract:", 10) != 0) {
		/*
		 * Backwards compatibility - try to open an Android control
		 * socket and if that fails, assume this was a UNIX domain
		 * socket instead.
		 */
		priv->sock = android_get_control_socket(ctrl);
		if (priv->sock >= 0) {
			wpa_printf(MSG_DEBUG,"Using Android control socket '%s'",ctrl);
			goto havesock;
		}
	}
#endif /* ANDROID */

	priv->sock = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (priv->sock < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_UNIX): %s", strerror(errno));
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
	addr.sun_len = sizeof(addr);
#endif /* __FreeBSD__ */
	addr.sun_family = AF_UNIX;

	if (os_strncmp(ctrl, "@abstract:", 10) == 0) {
		addr.sun_path[0] = '\0';
		os_strlcpy(addr.sun_path + 1, ctrl + 10,
			   sizeof(addr.sun_path) - 1);
		if (bind(priv->sock, (struct sockaddr *) &addr, sizeof(addr)) <
		    0) {
			wpa_printf(MSG_ERROR, "supp-global-ctrl-iface-init: " "bind(PF_UNIX;%s) failed: %s",ctrl, strerror(errno));
			goto fail;
		}
		wpa_printf(MSG_DEBUG, "Using Abstract control socket '%s'",ctrl + 10);
		goto havesock;
	}

	os_strlcpy(addr.sun_path, ctrl, sizeof(addr.sun_path));
	if (bind(priv->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_INFO, "supp-global-ctrl-iface-init(%s) (will try fixup): bind(PF_UNIX): %s",
			   ctrl, strerror(errno));
		if (connect(priv->sock, (struct sockaddr *) &addr,
			    sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(ctrl) < 0) {
				wpa_printf(MSG_ERROR,
					   "Could not unlink existing ctrl_iface socket '%s': %s",
					   ctrl, strerror(errno));
				goto fail;
			}
			if (bind(priv->sock, (struct sockaddr *) &addr,
				 sizeof(addr)) < 0) {
				wpa_printf(MSG_ERROR, "supp-glb-iface-init: bind(PF_UNIX;%s): %s",
					   ctrl, strerror(errno));
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'",
				   ctrl);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore",
				   ctrl);
			goto fail;
		}
	}

	wpa_printf(MSG_DEBUG, "Using UNIX control socket '%s'", ctrl);

	if (global->params.ctrl_interface_group) {
		char *gid_str = global->params.ctrl_interface_group;
		gid_t gid = 0;
		struct group *grp;
		char *endp;

		grp = getgrnam(gid_str);
		if (grp) {
			gid = grp->gr_gid;
			wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d"
				   " (from group name '%s')",
				   (int) gid, gid_str);
		} else {
			/* Group name not found - try to parse this as gid */
			gid = strtol(gid_str, &endp, 10);
			if (*gid_str == '\0' || *endp != '\0') {
				wpa_printf(MSG_ERROR, "CTRL: Invalid group "
					   "'%s'", gid_str);
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d",
				   (int) gid);
		}
		if (chown(ctrl, -1, gid) < 0) {
			wpa_printf(MSG_ERROR,
				   "chown[global_ctrl_interface=%s,gid=%d]: %s",
				   ctrl, (int) gid, strerror(errno));
			goto fail;
		}

		if (chmod(ctrl, S_IRWXU | S_IRWXG) < 0) {
			wpa_printf(MSG_ERROR,
				   "chmod[global_ctrl_interface=%s]: %s",
				   ctrl, strerror(errno));
			goto fail;
		}
	} else {
		chmod(ctrl, S_IRWXU);
	}

havesock:

	/*
	 * Make socket non-blocking so that we don't hang forever if
	 * target dies unexpectedly.
	 */
	flags = fcntl(priv->sock, F_GETFL);
	if (flags >= 0) {
		flags |= O_NONBLOCK;
		if (fcntl(priv->sock, F_SETFL, flags) < 0) {
			wpa_printf(MSG_INFO, "fcntl(ctrl, O_NONBLOCK): %s",strerror(errno));
			/* Not fatal, continue on.*/
		}
	}

	eloop_register_read_sock(priv->sock,wpa_supplicant_global_ctrl_iface_receive,global, priv);

	return priv;

fail:
	if (priv->sock >= 0)
		close(priv->sock);
	os_free(priv);
	return NULL;
}


static void wpa_supplicant_global_ctrl_iface_receive(int sock, void *eloop_ctx,void *sock_ctx)
{
	struct wpa_global *global = eloop_ctx;
	struct ctrl_iface_global_priv *priv = sock_ctx;
	char buf[256];
	int res;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	char *reply = NULL;
	size_t reply_len;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		wpa_printf(MSG_ERROR, "recvfrom(ctrl_iface): %s",strerror(errno));
		return;
	}
	buf[res] = '\0';

	

	if (os_strcmp(buf, "ATTACH") == 0) {
		if (wpa_supplicant_ctrl_iface_attach(&priv->ctrl_dst, &from,fromlen))
			reply_len = 1;
		else
			reply_len = 2;
	} else if (os_strcmp(buf, "DETACH") == 0) {
		if (wpa_supplicant_ctrl_iface_detach(&priv->ctrl_dst, &from,fromlen))
			reply_len = 1;
		else
			reply_len = 2;
	} else {
		reply = wpa_supplicant_global_ctrl_iface_process(global, buf,&reply_len);
	}

	if (reply) {
		sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from,fromlen);
		os_free(reply);
	} else if (reply_len == 1) {
		sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,fromlen);
	} else if (reply_len == 2) {
		sendto(sock, "OK\n", 3, 0, (struct sockaddr *) &from, fromlen);
	}
}



char * wpa_supplicant_global_ctrl_iface_process(struct wpa_global *global,char *buf, size_t *resp_len)
{
	char *reply;
	const int reply_size = 2048;
	int reply_len;
	int level = MSG_DEBUG;

	if (os_strncmp(buf, "IFNAME=", 7) == 0) {
		char *pos = os_strchr(buf + 7, ' ');
		if (pos) {
			*pos++ = '\0';
			return wpas_global_ctrl_iface_ifname(global,buf + 7, pos,resp_len);
		}
	}

	reply = wpas_global_ctrl_iface_redir(global, buf, resp_len);
	if (reply)
		return reply;

#ifdef REALTEK_WIFI_VENDOR
       if(os_strncmp(buf, "PING", 4) != 0)
               wpa_printf(MSG_INFO, "[CTRL_IFACE_G]%s", buf);
#endif

	if (os_strcmp(buf, "PING") == 0)
		level = MSG_EXCESSIVE;
	wpa_hexdump_ascii(level, "RX global ctrl_iface",(const u8 *) buf, os_strlen(buf));

	reply = os_malloc(reply_size);
	if (reply == NULL) {
		*resp_len = 1;
		return NULL;
	}

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (os_strncmp(buf, "INTERFACE_ADD ", 14) == 0) {
		if (wpa_supplicant_global_iface_add(global, buf + 14))
			reply_len = -1;
	} else if (os_strncmp(buf, "INTERFACE_REMOVE ", 17) == 0) {
		if (wpa_supplicant_global_iface_remove(global, buf + 17))
			reply_len = -1;
	} else if (os_strcmp(buf, "INTERFACE_LIST") == 0) {
		reply_len = wpa_supplicant_global_iface_list(
			global, reply, reply_size);
	} else if (os_strcmp(buf, "INTERFACES") == 0) {
		reply_len = wpa_supplicant_global_iface_interfaces(
			global, reply, reply_size);
	} else if (os_strcmp(buf, "TERMINATE") == 0) {
		wpa_supplicant_terminate_proc(global);
	} else if (os_strcmp(buf, "SUSPEND") == 0) {
		wpas_notify_suspend(global);
	} else if (os_strcmp(buf, "RESUME") == 0) {
		wpas_notify_resume(global);
	} else if (os_strncmp(buf, "SET ", 4) == 0) {
		if (wpas_global_ctrl_iface_set(global, buf + 4))
			reply_len = -1;
#ifndef CONFIG_NO_CONFIG_WRITE
	} else if (os_strcmp(buf, "SAVE_CONFIG") == 0) {
		if (wpas_global_ctrl_iface_save_config(global))
			reply_len = -1;
#endif /* CONFIG_NO_CONFIG_WRITE */
	} else if (os_strcmp(buf, "STATUS") == 0) {
		reply_len = wpas_global_ctrl_iface_status(global, reply,reply_size);
	} else {
		os_memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	*resp_len = reply_len;
	return reply;
}




//分析if (wpas_notify_supplicant_initialized(global)) {
int wpas_notify_supplicant_initialized(struct wpa_global *global)
{
#ifdef CONFIG_DBUS	//CONFIG_DBUS没有定义
	if (global->params.dbus_ctrl_interface) {
		global->dbus = wpas_dbus_init(global);
		if (global->dbus == NULL)
			return -1;
	}
#endif /* CONFIG_DBUS */

	return 0;
}


//分析if (wifi_display_init(global) < 0) {
int wifi_display_init(struct wpa_global *global)                                                                                                                          
{
	global->wifi_display = 1;
	return 0;
}


//分析
//exitcode = wpa_supplicant_run(global);

/**
 * wpa_supplicant_run - Run the %wpa_supplicant main event loop
 * @global: Pointer to global data from wpa_supplicant_init()
 * Returns: 0 after successful event loop run, -1 on failure
 *
 * This function starts the main event loop and continues running as long as
 * there are any remaining events. In most cases, this function is running as
 * long as the %wpa_supplicant process in still in use.
 */
int wpa_supplicant_run(struct wpa_global *global)
{
	struct wpa_supplicant *wpa_s;

	if (global->params.daemonize && wpa_supplicant_daemon(global->params.pid_file))  //global->params.daemonize == 0
		return -1;

	if (global->params.wait_for_monitor) { //	global->params.wait_for_monitor == 0
		for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next)
			if (wpa_s->ctrl_iface)
				wpa_supplicant_ctrl_iface_wait(wpa_s->ctrl_iface);
	}

	eloop_register_signal_terminate(wpa_supplicant_terminate, global);
	eloop_register_signal_reconfig(wpa_supplicant_reconfig, global);

	eloop_run();

	return 0;
}












