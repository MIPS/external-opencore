LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/pvmf_parsernode_impl.cpp \
 	src/pvmf_parsernode_port.cpp \
 	src/pvmf_parsernode_factory.cpp


LOCAL_MODULE := libpvcommonparsernode

LOCAL_CFLAGS :=   $(PV_CFLAGS)


LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/nodes/pvcommonparsernode/src \
 	$(PV_TOP)/nodes/pvcommonparsernode/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
	include/pvmf_parsernode_factory.h \
 	include/pvmf_parsernode_impl.h \
 	include/pvmf_parsernode_port.h

include $(BUILD_STATIC_LIBRARY)
