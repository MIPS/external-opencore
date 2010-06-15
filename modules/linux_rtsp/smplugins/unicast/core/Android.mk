LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
 	src/pvmf_smnode_rtspunicastmanager_plugin_interface.cpp


LOCAL_MODULE := libpvrtspsmplugininterface

LOCAL_CFLAGS :=   $(PV_CFLAGS)


LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/modules/linux_rtsp/smplugins/unicast/core/src \
 	$(PV_TOP)/modules/linux_rtsp/smplugins/unicast/core/build/make \
 	$(PV_TOP)/oscl/oscl/oscllib/src \
 	$(PV_TOP)/nodes/streaming/streamingmanager/plugins/rtspunicast/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	

include $(BUILD_STATIC_LIBRARY)
