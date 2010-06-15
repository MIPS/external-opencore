LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
 	src/pvmf_smnode_rtsptstreaming_plugin_registry_populator_interface.cpp


LOCAL_MODULE := libpvsmrtsptstreamingpluginreg

LOCAL_CFLAGS :=   $(PV_CFLAGS)


LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/modules/linux_rtsp/smplugins/tunneling/registry/src \
 	$(PV_TOP)/modules/linux_rtsp/smplugins/tunneling/registry/build/make \
 	$(PV_TOP)/oscl/oscl/oscllib/src \
 	$(PV_TOP)/nodes/streaming/streamingmanager/plugins/rtsptunicast/include \
 	$(PV_TOP)/nodes/streaming/streamingmanager/plugins/common/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	

include $(BUILD_STATIC_LIBRARY)
