# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pv2waysample 


XCXXFLAGS += $(FLAG_COMPILE_WARNINGS_AS_ERRORS)


XINCDIRS +=  ../../src ../../include \
 ../../../../../../nodes/pvdummyinputnode/include \
 ../../../../../../nodes/pvdummyoutputnode/include \
 ../../../../../../protocols/systems/common/include \
 ../../../../../../nodes/pvclientserversocketnode/include \
../../../../test/include


ifeq ($(HOST_ARCH),win32)
ifeq ($(GUI_APP),1)
XINCDIRS += ../../../gui_based/src
endif
endif


SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := ../../../test/src/lipsync_dummy_input_mio.cpp \
        ../../../test/src/lipsync_dummy_output_mio.cpp \
	pv_2way_media_input_mio_node_factory.cpp \
	pv_2way_media_output_mio_node_factory.cpp \
	pv_2way_dummy_input_mio_node_factory.cpp \
	pv_2way_dummy_output_mio_node_factory.cpp \
	pv_2way_source_and_sinks_base.cpp \
	pv_2way_source_and_sinks_file.cpp \
	pv_2way_source_and_sinks_dummy.cpp \
	pv_2way_source_and_sinks_perf_test.cpp \
	pv_2way_media.cpp \
	pv_2way_mio.cpp \
	pv_2way_sink.cpp \
	pv_2way_source.cpp \
	twowaysocket.cpp 
	

ifeq ($(HOST_ARCH),win32)
ifeq ($(GUI_APP),1)
SRCS += pv_2way_source_and_sinks.cpp \
	pv_2way_win_audio_mio_node_factory.cpp \
        pv_2way_win_video_mio_node_factory.cpp \
        pv_2way_win_video_mio_node.cpp \
        pv_2way_modem.cpp \
        ../../gui_based/src/pv_com_3g_modem.cpp
endif
endif
 

#ifeq (0,1)
#SRCS +=	pv_2way_dummy_output_mio_node_factory.cpp \
#	pv_2way_dummy_input_mio_node_factory.cpp 
#endif


HDRS := pv_2way_codecspecifier.h \
        pv_2way_codecspecifier_interface.h \
        pv_2way_media_input_mio_node_factory.h \
	pv_2way_media_output_mio_node_factory.h \
	pv_2way_dummy_input_mio_node_factory.h \
	pv_2way_dummy_output_mio_node_factory.h \
	pv_2way_source_and_sinks_base.h \
	pv_2way_source_and_sinks_file.h \
	pv_2way_source_and_sinks_dummy.h \
	pv_2way_mio.h \
	pv_2way_mio_node_factory.h \
	twowaysocket.h \
        pv2way_file_names.h


include $(MK)/library.mk

