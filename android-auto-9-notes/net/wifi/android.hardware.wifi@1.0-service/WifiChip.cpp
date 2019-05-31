//  @ /work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/wifi/1.2/default/wifi_chip.cpp


// 这里是在wifi类中的 startInternal 中 创建
WifiChip::WifiChip(
    ChipId chip_id, const std::weak_ptr<legacy_hal::WifiLegacyHal> legacy_hal,
    const std::weak_ptr<mode_controller::WifiModeController> mode_controller,
    const std::weak_ptr<feature_flags::WifiFeatureFlags> feature_flags)
    : chip_id_(chip_id),
      legacy_hal_(legacy_hal),
      mode_controller_(mode_controller),
      feature_flags_(feature_flags),
      is_valid_(true),
      current_mode_id_(kInvalidModeId),
      debug_ring_buffer_cb_registered_(false) {
    populateModes();
}


void WifiChip::populateModes() {
    // The chip combination supported for current devices is fixed.
    // They can be one of the following based on device features:
    // a) 2 separate modes of operation with 1 interface combination each:
    //    Mode 1 (STA mode): Will support 1 STA and 1 P2P or NAN(optional)
    //                       concurrent iface operations.
    //    Mode 2 (AP mode): Will support 1 AP iface operation.
    //
    // b) 1 mode of operation with 2 interface combinations
    // (conditional on isDualInterfaceSupported()):
    //    Interface Combination 1: Will support 1 STA and 1 P2P or NAN(optional)
    //                             concurrent iface operations.
    //    Interface Combination 2: Will support 1 STA and 1 AP concurrent
    //                             iface operations.2
    // If Aware is enabled (conditional on isAwareSupported()), the iface
    // combination will be modified to support either P2P or NAN in place of
    // just P2P.
    if (feature_flags_.lock()->isDualInterfaceSupported()) {  // true
        // V2 Iface combinations for Mode Id = 2.
        const IWifiChip::ChipIfaceCombinationLimit chip_iface_combination_limit_1 = { {IfaceType::STA}, 1 };
        const IWifiChip::ChipIfaceCombinationLimit chip_iface_combination_limit_2 = { {IfaceType::AP}, 1 };
        IWifiChip::ChipIfaceCombinationLimit chip_iface_combination_limit_3;
        if (feature_flags_.lock()->isAwareSupported()) {  // false
            ......
        } else {
            chip_iface_combination_limit_3 = { {IfaceType::P2P}, 1 };
        }
        const IWifiChip::ChipIfaceCombination chip_iface_combination_1 = { {chip_iface_combination_limit_1, chip_iface_combination_limit_2} };
        const IWifiChip::ChipIfaceCombination chip_iface_combination_2 = { {chip_iface_combination_limit_1, chip_iface_combination_limit_3} };
        if (feature_flags_.lock()->isApDisabled()) { // false
          ......
        } else {
          const IWifiChip::ChipMode chip_mode = { kV2ChipModeId, {chip_iface_combination_1, chip_iface_combination_} };
          modes_ = {chip_mode};
        }
    } else {
       ......
    }
}