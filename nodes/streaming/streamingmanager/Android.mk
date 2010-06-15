LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/pvmf_sm_node_factory.cpp \
 	src/pvmf_streaming_manager_node.cpp \
 	src/pvmf_sm_fsp_registry.cpp


LOCAL_MODULE := libpvstreamingmanagernode

LOCAL_CFLAGS :=   $(PV_CFLAGS)


LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/nodes/streaming/streamingmanager/src \
 	$(PV_TOP)/nodes/streaming/streamingmanager/include \
 	$(PV_TOP)/nodes/streaming/streamingmanager/plugins/common/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
	include/pvmf_sm_node_events.h \
 	include/pvmf_sm_node_factory.h \
 	include/pvmf_sm_fsp_registry_interface.h \
 	include/pvmf_sm_fsp_registry_populator_interface.h \
 	include/pvmf_sm_fsp_shared_lib_interface.h

include $(BUILD_STATIC_LIBRARY)
