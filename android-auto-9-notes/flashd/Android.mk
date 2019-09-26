LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := flashd

LOCAL_SRC_FILES := \
	main.cpp \
	FlashService.cpp \
	NetlinkManager.cpp \
	NetlinkHandler.cpp

#LOCAL_VENDOR_MODULE := true

LOCAL_INIT_RC := flashd.rc

LOCAL_C_INCLUDES := \
	external/libusb/libusb \

LOCAL_CFLAGS := -Wall -Wextra -Werror -Wno-unused-parameter

LOCAL_SHARED_LIBRARIES := \
                    liblog \
                    libusb \
                    libbase \
                    libutils \
                    libcutils \
                    libsysutils \

include $(BUILD_EXECUTABLE)
