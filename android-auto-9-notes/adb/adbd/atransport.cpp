

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/system/core/adb/transport.cpp

atransport::atransport(ConnectionState state = kCsOffline)
    : id(NextTransportId()), connection_state_(state) {
    transport_fde = {};
    // Initialize protocol to min version for compatibility with older versions.
    // Version will be updated post-connect.
    protocol_version = A_VERSION_MIN;
    max_payload = MAX_PAYLOAD;
}