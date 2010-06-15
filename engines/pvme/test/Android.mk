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
 	$(PV_TOP)/engines/player/include \
 	$(PV_TOP)/extern_libs_v2/wmdrm/common/include \
 	$(PV_TOP)/extern_libs_v2/wmdrm/playready/include \
 	$(PV_TOP)/pvmi/content_policy_manager/plugins/common/include \
 	$(PV_TOP)/engines/playready_utility/include \
 	$(PV_TOP)/pvmi/content_policy_manager/plugins/playready/include \
 	$(PV_TOP)/engines/pvme/test/config/android \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	

-include $(PV_TOP)/Android_system_extras.mk

include $(BUILD_EXECUTABLE)
