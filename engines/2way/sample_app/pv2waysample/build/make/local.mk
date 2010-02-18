# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

include $(LOCAL_PATH)/2way_$(HOST_ARCH).mk

TARGET := pv2waysample 




XINCDIRS +=  ../../src ../../include \
 ../../../../../../protocols/systems/common/include \
 ../../../../../../nodes/pvclientserversocketnode/include \
../../../../test/include


SRCDIR := ../../src
INCSRCDIR := ../../include


SRCS :=	\
    lipsync_dummy_input_mio.cpp \
    lipsync_dummy_output_mio.cpp \
    dummy_input_mio.cpp \
    dummy_output_mio.cpp \
    pv_2way_lipsync_input_mio_node_factory.cpp \
    pv_2way_lipsync_output_mio_node_factory.cpp \
    pv_2way_dummy_input_mio_node_factory.cpp \
    pv_2way_dummy_output_mio_node_factory.cpp \
    pv_2way_media_input_mio_node_factory.cpp \
    pv_2way_media_output_mio_node_factory.cpp \
    pv_2way_source_and_sinks_base.cpp \
    pv_2way_source_and_sinks_file.cpp \
    pv_2way_source_and_sinks_dummy.cpp \
    pv_2way_source_and_sinks_lipsync.cpp \
    pv_2way_media.cpp \
    pv_2way_mio.cpp \
    pv_2way_sink.cpp \
    pv_2way_source.cpp \
    twowaysocket.cpp 
	

SRCS += $(EXTRASRCS)
 

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
	pv_2way_lipsync_input_mio_node_factory.h \
	pv_2way_lipsync_output_mio_node_factory.h \
	pv_2way_source_and_sinks_base.h \
	pv_2way_source_and_sinks_file.h \
	pv_2way_source_and_sinks_dummy.h \
	pv_2way_source_and_sinks_lipsync.h \
	pv_2way_mio.h \
	pv_2way_mio_node_factory.h \
	twowaysocket.h \
        pv2way_file_names.h


include $(MK)/library.mk

