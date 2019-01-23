



static char const* NetdNativeService::getServiceName() { return "netd"; }



binder::Status NetdNativeService::setResolverConfiguration(int32_t netId,
        const std::vector<std::string>& servers, const std::vector<std::string>& domains,
        const std::vector<int32_t>& params, const std::string& tlsName,
        const std::vector<std::string>& tlsServers,
        const std::vector<std::string>& tlsFingerprints) {
    // This function intentionally does not lock within Netd, as Bionic is thread-safe.
    ENFORCE_PERMISSION(CONNECTIVITY_INTERNAL);

    std::set<std::vector<uint8_t>> decoded_fingerprints;
    for (const std::string& fingerprint : tlsFingerprints) {
        std::vector<uint8_t> decoded = parseBase64(fingerprint);
        if (decoded.empty()) {
            return binder::Status::fromServiceSpecificError(EINVAL,String8::format("ResolverController error: bad fingerprint"));
        }
        decoded_fingerprints.emplace(decoded);
    }

    int err = gCtls->resolverCtrl.setResolverConfiguration(netId, servers, domains, params,tlsName, tlsServers, decoded_fingerprints);
    if (err != 0) {
        return binder::Status::fromServiceSpecificError(-err,String8::format("ResolverController error: %s", strerror(-err)));
    }
    return binder::Status::ok();
}