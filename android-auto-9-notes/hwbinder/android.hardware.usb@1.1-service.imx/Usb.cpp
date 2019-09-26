//  @   vendor/nxp-opensource/imx/usb/Usb.cpp
Usb::Usb() : mLock(PTHREAD_MUTEX_INITIALIZER),
    mRoleSwitchLock(PTHREAD_MUTEX_INITIALIZER),
    mPartnerLock(PTHREAD_MUTEX_INITIALIZER),
    mPartnerUp(false) {
    pthread_condattr_t attr;
    if (pthread_condattr_init(&attr)) {
        ALOGE("pthread_condattr_init failed: %s", strerror(errno));
        abort();
    }
    if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) {
        ALOGE("pthread_condattr_setclock failed: %s", strerror(errno));
        abort();
    }
    if (pthread_cond_init(&mPartnerCV, &attr))  {
        ALOGE("pthread_cond_init failed: %s", strerror(errno));
        abort();
    }
    if (pthread_condattr_destroy(&attr)) {
        ALOGE("pthread_condattr_destroy failed: %s", strerror(errno));
        abort();
    }
}

/**
 * Used as a container to send port role information.
 */
struct PortRole {
    /**
     * Indicates the type of Port Role.
     * Maps to the PortRoleType enum.
     */
    PortRoleType type;

    /**
     * when type is HAL_USB_DATA_ROLE pass values from enum PortDataRole.
     * when type is HAL_USB_POWER_ROLE pass values from enum PortPowerRole.
     * when type is HAL_USB_MODE pass values from enum PortMode.
     */
    uint32_t role;
};

std::string appendRoleNodeHelper(const std::string &portName,PortRoleType type) {
  std::string node("/sys/class/typec/" + portName);

  switch (type) {
    case PortRoleType::DATA_ROLE:
      return node + "/data_role";
    case PortRoleType::POWER_ROLE:
      return node + "/power_role";
    case PortRoleType::MODE:
      return node + "/port_type";
    default:
      return "";
  }
}

Return<void> Usb::switchRole(const hidl_string &portName, const V1_0::PortRole &newRole) {
    /**
     * /sys/class/typec/ + portName + [/data_role,/power_role,/port_type]
    */
  std::string filename = appendRoleNodeHelper(std::string(portName.c_str()), newRole.type);
  std::string written;
  FILE *fp;
  bool roleSwitch = false;

  if (filename == "") {
    ALOGE("Fatal: invalid node type");
    return Void();
  }

  pthread_mutex_lock(&mRoleSwitchLock);

  ALOGI("filename write: %s role:%s", filename.c_str(),convertRoletoString(newRole).c_str());

  if (newRole.type == PortRoleType::MODE) {
      roleSwitch = switchMode(portName, newRole, this);
  } else {
    fp = fopen(filename.c_str(), "w");
    if (fp != NULL) {
      int ret = fputs(convertRoletoString(newRole).c_str(), fp);
      fclose(fp);
      if ((ret != EOF) && !readFile(filename, &written)) {
        extractRole(&written);
        ALOGI("written: %s", written.c_str());
        if (written == convertRoletoString(newRole)) {
          roleSwitch = true;
        } else {
          ALOGE("Role switch failed");
        }
      } else {
        ALOGE("failed to update the new role");
      }
    } else {
      ALOGE("fopen failed");
    }
  }

  pthread_mutex_lock(&mLock);
  if (mCallback_1_0 != NULL) {
    Return<void> ret = mCallback_1_0->notifyRoleSwitchStatus(portName, newRole,roleSwitch ? Status::SUCCESS : Status::ERROR);
    if (!ret.isOk())
      ALOGE("RoleSwitchStatus error %s", ret.description().c_str());
  } else {
    ALOGE("Not notifying the userspace. Callback is not set");
  }
  pthread_mutex_unlock(&mLock);
  pthread_mutex_unlock(&mRoleSwitchLock);

  return Void();
}
  
  
  
  
  
  
  
  
  
Return<void> Usb::queryPortStatus() {
  hidl_vec<PortStatus_1_1> currentPortStatus_1_1;
  hidl_vec<V1_0::PortStatus> currentPortStatus;
  Status status;
  sp<IUsbCallback> callback_V1_1 = IUsbCallback::castFrom(mCallback_1_0);

  pthread_mutex_lock(&mLock);
  if (mCallback_1_0 != NULL) {
    if (callback_V1_1 != NULL) {
      status = getPortStatusHelper(&currentPortStatus_1_1, false);
    } else {
      status = getPortStatusHelper(&currentPortStatus_1_1, true);
      currentPortStatus.resize(currentPortStatus_1_1.size());
      for (unsigned long i = 0; i < currentPortStatus_1_1.size(); i++)
        currentPortStatus[i] = currentPortStatus_1_1[i].status;
    }

    Return<void> ret;

    if (callback_V1_1 != NULL)
      ret = callback_V1_1->notifyPortStatusChange_1_1(currentPortStatus_1_1, status);
    else
      ret = mCallback_1_0->notifyPortStatusChange(currentPortStatus, status);

    if (!ret.isOk())
      ALOGE("queryPortStatus_1_1 error %s", ret.description().c_str());
  } else {
    ALOGI("Notifying userspace skipped. Callback is NULL");
  }
  pthread_mutex_unlock(&mLock);

  return Void();
}