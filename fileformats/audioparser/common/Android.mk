LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/pvmf_audio_parser.cpp \
 	src/pvmf_file_parser.cpp


LOCAL_MODULE := libaudioparser

LOCAL_CFLAGS :=   $(PV_CFLAGS)


LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/fileformats/audioparser/common/src \
 	$(PV_TOP)/fileformats/audioparser/common/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
	include/pvmf_audio_parser.h \
 	include/pvmf_fileparser_interface.h \
 	include/pvmf_fileparser_observer.h

include $(BUILD_STATIC_LIBRARY)
