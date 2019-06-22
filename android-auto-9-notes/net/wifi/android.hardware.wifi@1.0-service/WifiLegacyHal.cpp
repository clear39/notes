//  @   /work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/wifi/1.2/default/wifi_legacy_hal.cpp
WifiLegacyHal::WifiLegacyHal()
    : global_handle_(nullptr),
      awaiting_event_loop_termination_(false),
      is_started_(false) {
      LOG(DEBUG) << "WifiLegacyHal create";
}


// 这里是在wifi类中的 startInternal-------> initializeModeControllerAndLegacyHal 中调用
wifi_error WifiLegacyHal::initialize() {
    LOG(DEBUG) << "Initialize legacy HAL";
    // TODO: Add back the HAL Tool if we need to. All we need from the HAL tool
    // for now is this function call which we can directly call.
    //  @   /work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/wifi/1.2/default/wifi_legacy_hal_stubs.cpp
    //  这里只是将 global_func_table_ 所有函数指针 赋值为一个固定的 指针函数 ，返回值为不支持
    if (!initHalFuncTableWithStubs(&global_func_table_)) {
        LOG(ERROR) << "Failed to initialize legacy hal function table with stubs";
        return WIFI_ERROR_UNKNOWN;
    }
    /**
     * @    /work/workcodes/aosp-p9.x-auto-alpha/hardware/qcom/wlan/qcwcn/wifi_hal/wifi_hal.cpp
     * 
     * 在 /work/workcodes/aosp-p9.x-auto-alpha/frameworks/opt/net/wifi/libwifi_hal/Android.mk 中重新导入了新的 对应的库
     */ 
    wifi_error status = init_wifi_vendor_hal_func_table(&global_func_table_);
    if (status != WIFI_SUCCESS) {
        LOG(ERROR) << "Failed to initialize legacy hal function table";
    }
    return status;
}

//  @    /work/workcodes/aosp-p9.x-auto-alpha/hardware/qcom/wlan/qcwcn/wifi_hal/wifi_hal.cpp
/*initialize function pointer table with Qualcomm HAL API*/
wifi_error init_wifi_vendor_hal_func_table(wifi_hal_fn *fn) {
    if (fn == NULL) {
        return WIFI_ERROR_UNKNOWN;
    }

    fn->wifi_initialize = wifi_initialize;
    fn->wifi_wait_for_driver_ready = wifi_wait_for_driver_ready;
    fn->wifi_cleanup = wifi_cleanup;
    fn->wifi_event_loop = wifi_event_loop;
    fn->wifi_get_supported_feature_set = wifi_get_supported_feature_set;
    fn->wifi_get_concurrency_matrix = wifi_get_concurrency_matrix;
    fn->wifi_set_scanning_mac_oui =  wifi_set_scanning_mac_oui;
    fn->wifi_get_ifaces = wifi_get_ifaces;
    fn->wifi_get_iface_name = wifi_get_iface_name;
    fn->wifi_set_iface_event_handler = wifi_set_iface_event_handler;
    fn->wifi_reset_iface_event_handler = wifi_reset_iface_event_handler;
    fn->wifi_start_gscan = wifi_start_gscan;
    fn->wifi_stop_gscan = wifi_stop_gscan;
    fn->wifi_get_cached_gscan_results = wifi_get_cached_gscan_results;
    fn->wifi_set_bssid_hotlist = wifi_set_bssid_hotlist;
    fn->wifi_reset_bssid_hotlist = wifi_reset_bssid_hotlist;
    fn->wifi_set_significant_change_handler = wifi_set_significant_change_handler;
    fn->wifi_reset_significant_change_handler = wifi_reset_significant_change_handler;
    fn->wifi_get_gscan_capabilities = wifi_get_gscan_capabilities;
    fn->wifi_set_link_stats = wifi_set_link_stats;
    fn->wifi_get_link_stats = wifi_get_link_stats;
    fn->wifi_clear_link_stats = wifi_clear_link_stats;
    fn->wifi_get_valid_channels = wifi_get_valid_channels;
    fn->wifi_rtt_range_request = wifi_rtt_range_request;
    fn->wifi_rtt_range_cancel = wifi_rtt_range_cancel;
    fn->wifi_get_rtt_capabilities = wifi_get_rtt_capabilities;
    fn->wifi_rtt_get_responder_info = wifi_rtt_get_responder_info;
    fn->wifi_enable_responder = wifi_enable_responder;
    fn->wifi_disable_responder = wifi_disable_responder;
    fn->wifi_set_nodfs_flag = wifi_set_nodfs_flag;
    fn->wifi_start_logging = wifi_start_logging;
    fn->wifi_set_epno_list = wifi_set_epno_list;
    fn->wifi_reset_epno_list = wifi_reset_epno_list;
    fn->wifi_set_country_code = wifi_set_country_code;
    fn->wifi_enable_tdls = wifi_enable_tdls;
    fn->wifi_disable_tdls = wifi_disable_tdls;
    fn->wifi_get_tdls_status = wifi_get_tdls_status;
    fn->wifi_get_tdls_capabilities = wifi_get_tdls_capabilities;
    fn->wifi_get_firmware_memory_dump = wifi_get_firmware_memory_dump;
    fn->wifi_set_log_handler = wifi_set_log_handler;
    fn->wifi_reset_log_handler = wifi_reset_log_handler;
    fn->wifi_set_alert_handler = wifi_set_alert_handler;
    fn->wifi_reset_alert_handler = wifi_reset_alert_handler;
    fn->wifi_get_firmware_version = wifi_get_firmware_version;
    fn->wifi_get_ring_buffers_status = wifi_get_ring_buffers_status;
    fn->wifi_get_logger_supported_feature_set = wifi_get_logger_supported_feature_set;
    fn->wifi_get_ring_data = wifi_get_ring_data;
    fn->wifi_get_driver_version = wifi_get_driver_version;
    fn->wifi_set_passpoint_list = wifi_set_passpoint_list;
    fn->wifi_reset_passpoint_list = wifi_reset_passpoint_list;
    fn->wifi_set_lci = wifi_set_lci;
    fn->wifi_set_lcr = wifi_set_lcr;
    fn->wifi_start_sending_offloaded_packet = wifi_start_sending_offloaded_packet;
    fn->wifi_stop_sending_offloaded_packet = wifi_stop_sending_offloaded_packet;
    fn->wifi_start_rssi_monitoring = wifi_start_rssi_monitoring;
    fn->wifi_stop_rssi_monitoring = wifi_stop_rssi_monitoring;
    fn->wifi_nan_enable_request = nan_enable_request;
    fn->wifi_nan_disable_request = nan_disable_request;
    fn->wifi_nan_publish_request = nan_publish_request;
    fn->wifi_nan_publish_cancel_request = nan_publish_cancel_request;
    fn->wifi_nan_subscribe_request = nan_subscribe_request;
    fn->wifi_nan_subscribe_cancel_request = nan_subscribe_cancel_request;
    fn->wifi_nan_transmit_followup_request = nan_transmit_followup_request;
    fn->wifi_nan_stats_request = nan_stats_request;
    fn->wifi_nan_config_request = nan_config_request;
    fn->wifi_nan_tca_request = nan_tca_request;
    fn->wifi_nan_beacon_sdf_payload_request = nan_beacon_sdf_payload_request;
    fn->wifi_nan_register_handler = nan_register_handler;
    fn->wifi_nan_get_version = nan_get_version;
    fn->wifi_set_packet_filter = wifi_set_packet_filter;
    fn->wifi_get_packet_filter_capabilities = wifi_get_packet_filter_capabilities;
    fn->wifi_read_packet_filter = wifi_read_packet_filter;
    fn->wifi_nan_get_capabilities = nan_get_capabilities;
    fn->wifi_nan_data_interface_create = nan_data_interface_create;
    fn->wifi_nan_data_interface_delete = nan_data_interface_delete;
    fn->wifi_nan_data_request_initiator = nan_data_request_initiator;
    fn->wifi_nan_data_indication_response = nan_data_indication_response;
    fn->wifi_nan_data_end = nan_data_end;
    fn->wifi_configure_nd_offload = wifi_configure_nd_offload;
    fn->wifi_get_driver_memory_dump = wifi_get_driver_memory_dump;
    fn->wifi_get_wake_reason_stats = wifi_get_wake_reason_stats;
    fn->wifi_start_pkt_fate_monitoring = wifi_start_pkt_fate_monitoring;
    fn->wifi_get_tx_pkt_fates = wifi_get_tx_pkt_fates;
    fn->wifi_get_rx_pkt_fates = wifi_get_rx_pkt_fates;
    fn->wifi_get_roaming_capabilities = wifi_get_roaming_capabilities;
    fn->wifi_configure_roaming = wifi_configure_roaming;
    fn->wifi_enable_firmware_roaming = wifi_enable_firmware_roaming;
    fn->wifi_select_tx_power_scenario = wifi_select_tx_power_scenario;
    fn->wifi_reset_tx_power_scenario = wifi_reset_tx_power_scenario;
    fn->wifi_set_radio_mode_change_handler = wifi_set_radio_mode_change_handler;

    return WIFI_SUCCESS;
}



/**
 * 
 * 这里是在WifiChip中调用
 */ 
wifi_error WifiLegacyHal::start() {
    // Ensure that we're starting in a good state.
    CHECK(global_func_table_.wifi_initialize && !global_handle_ && iface_name_to_handle_.empty() && !awaiting_event_loop_termination_);
    if (is_started_) {
        LOG(DEBUG) << "Legacy HAL already started";
        return WIFI_SUCCESS;
    }
    LOG(DEBUG) << "Waiting for the driver ready";
    /**
     * 这里 wifi_wait_for_driver_ready 回去不断以读的模式打开 /sys/class/net/wlan0 目录，
     * 如果目录存在可读则为成功返回成功；否则返回失败
     */ 
    wifi_error status = global_func_table_.wifi_wait_for_driver_ready();
    if (status == WIFI_ERROR_TIMED_OUT) {
        LOG(ERROR) << "Timed out awaiting driver ready";
        return status;
    }
    LOG(DEBUG) << "Starting legacy HAL";
    /**
     * @    /work/workcodes/aosp-p9.x-auto-alpha/frameworks/opt/net/wifi/libwifi_system_iface/interface_tool.cpp
     * 
     */ 
    if (!iface_tool_.SetWifiUpState(true)) {
        LOG(ERROR) << "Failed to set WiFi interface up";
        return WIFI_ERROR_UNKNOWN;
    }
    /**
     * 
     */
    status = global_func_table_.wifi_initialize(&global_handle_);
    if (status != WIFI_SUCCESS || !global_handle_) {
        LOG(ERROR) << "Failed to retrieve global handle";
        return status;
    }
    std::thread(&WifiLegacyHal::runEventLoop, this).detach();
    status = retrieveIfaceHandles();
    if (status != WIFI_SUCCESS || iface_name_to_handle_.empty()) {
        LOG(ERROR) << "Failed to retrieve wlan interface handle";
        return status;
    }
    LOG(DEBUG) << "Legacy HAL start complete";
    is_started_ = true;
    return WIFI_SUCCESS;
}