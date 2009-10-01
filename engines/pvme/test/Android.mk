LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/test_pvme.cpp \
 	src/test_pvme_testset1.cpp


LOCAL_MODULE := pvme_test

LOCAL_CFLAGS :=   $(PV_CFLAGS)


LOCAL_STATIC_LIBRARIES := libunit_test   

LOCAL_SHARED_LIBRARIES :=  libopencore_player libopencore_common libopencore_pvme

LOCAL_C_INCLUDES := \
	$(PV_TOP)/engines/pvme/test/src \
 	$(PV_TOP)/engines/pvme/test/src \
 	$(PV_TOP)/engines/pvme/test/config/default \
 	$(PV_TOP)/engines/player/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	

-include $(PV_TOP)/Android_system_extras.mk

include $(BUILD_EXECUTABLE)
