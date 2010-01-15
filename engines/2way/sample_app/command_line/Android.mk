LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/main.cpp \
 	src/pv_2way_console_source_and_sinks.cpp \
 	src/console_engine_handler.cpp \
 	src/../../pv2waysample/src/testcaseparser.cpp \
 	src/../../../../../protocols/systems/tools/general/common/src/test_utility.cpp \
 	src/kbhit.cpp


LOCAL_MODULE := pv2way_console_app

LOCAL_CFLAGS := -DPV_USE_AMR_CODECS  $(PV_CFLAGS)


LOCAL_STATIC_LIBRARIES :=               libunit_test 

LOCAL_SHARED_LIBRARIES := libopencore_2way               libopencore_common

LOCAL_C_INCLUDES := \
	$(PV_TOP)/engines/2way/sample_app/command_line/src \
 	$(PV_TOP)/engines/2way/sample_app/command_line/include \
 	$(PV_TOP)/engines/2way/sample_app/src \
 	$(PV_TOP)/engines/2way/sample_app/include \
 	$(PV_TOP)/engines/2way/sample_app/pv2waysample/include \
 	$(PV_TOP)/engines/common/include \
 	$(PV_TOP)/protocols/systems/common/include \
 	$(PV_TOP)/engines/2way/test/include \
 	$(PV_TOP)/protocols/systems/tools/general/common/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	

-include $(PV_TOP)/Android_system_extras.mk

include $(BUILD_EXECUTABLE)
