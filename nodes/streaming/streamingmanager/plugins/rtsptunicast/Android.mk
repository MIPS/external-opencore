LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/pvmf_sm_fsp_rtspt_unicast.cpp \
 	src/pvmf_sm_rtspt_unicast_node_factory.cpp


LOCAL_MODULE := libpvrtsptunicaststreamingmanager

LOCAL_CFLAGS := -DPV_CPU_ARCH_VERSION=0  $(PV_CFLAGS)


LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/nodes/streaming/streamingmanager/plugins/rtsptunicast/src \
 	$(PV_TOP)/nodes/streaming/streamingmanager/plugins/rtsptunicast/include \
 	$(PV_TOP)/nodes/streaming/streamingmanager/plugins/common/include \
 	$(PV_TOP)/nodes/streaming/common/include \
 	$(PV_TOP)/nodes/streaming/jitterbuffernode/jitterbuffer/common/include \
 	$(PV_TOP)/protocols/rtsp_client_engine/inc \
 	$(PV_TOP)/protocols/rtsp_client_engine/src \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	include/pvmf_sm_rtspt_unicast_node_factory.h

include $(BUILD_STATIC_LIBRARY)
