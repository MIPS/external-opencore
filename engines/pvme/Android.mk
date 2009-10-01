LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/pv_metadata_engine.cpp \
 	src/pv_metadata_engine_factory.cpp \
 	src/pvme_node_registry.cpp \
 	src/../config/default/pvme_node_registry_populator.cpp


LOCAL_MODULE := libpvmetadata_engine

LOCAL_CFLAGS :=   $(PV_CFLAGS)


LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/engines/pvme/src \
 	$(PV_TOP)/engines/pvme/include \
 	$(PV_TOP)/engines/player/include \
 	$(PV_TOP)/engines/player/src \
 	$(PV_TOP)/engines/player/config/core \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
	include/pv_metadata_engine.h \
 	include/pv_metadata_engine_factory.h \
 	include/pv_metadata_engine_interface.h \
 	include/pv_metadata_engine_types.h \
 	include/pvme_node_registry.h

include $(BUILD_STATIC_LIBRARY)
