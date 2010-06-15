# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pv2way_engine_test
TWOWAY_TARGET := pv2way_engine_test

ifdef twoway_config
include $(call process_include_list,$(LOCAL_PATH),$(twoway_config)).mk
endif

include $(LOCAL_PATH)/2way_$(HOST_ARCH).mk

XCPPFLAGS += $(LIP_SYNC_ENABLED)

SRCDIR := ../../src
INCSRCDIR := ../../include 
			
XINCDIRS += \
    ../../../sample_app/pv2waysample/include \
    ../../../../../protocols/systems/common/include \
    ../../../include \
    ../../../../../protocols/systems/tools/general/common/include 
    
SRCS := \
    alloc_dealloc_test.cpp \
	av_duplicate_test.cpp \
	test_engine.cpp \
	engine_test.cpp \
	init_cancel_test.cpp \
	init_test.cpp \
	test_base.cpp \
	../../../../protocols/systems/tools/general/common/src/test_utility.cpp  

SRCS_324 = \
    av_test.cpp \
    av_using_test_extension.cpp \
	acceptable_formats_test.cpp \
	negotiated_formats_test.cpp \
    turn_on_test_buffer_alloc.cpp \
	connect_cancel_test.cpp \
	connect_test.cpp \
	audio_only_test.cpp \
	reconnect_test.cpp \
	video_only_test.cpp \
	user_input_test.cpp \
	basic_lipsync_test.cpp \
	pause_resume_test.cpp \
    receive_data_test.cpp \
    error_check_audio_only_test.cpp

SRCS += $(SRCS_324)

BASE_LIBS = \
   pv2waysample \
   pv2wayengine \
   pv324m 

END_LIBS = \
   pvclientserversocketnode \
   pvmediainputnode \
   pvmediaoutputnode \
   pvmiofileinput \
   pvmiofileoutput\
   pvmf \
   colorconvert \
   pvdecoder_gsmamr \
   pvencoder_gsmamr \
   pvmediadatastruct \
   pv_amr_nb_common_lib \
   pvamrwbdecoder \
   pvmp4decoder \
   pvm4vencoder \
   pvavcdecoder \
   pvavch264enc \
   pv_avc_common_lib \
   pvcommsionode \
   pvgendatastruct \
   pvgeneraltools \
   pvmimeutils \
   pvthreadmessaging \
   pvlatmpayloadparser \
   pvmio_comm_loopback \
   unit_test_utils \
   unit_test \
   pvlogger \
   osclio \
   osclproc \
   osclutil \
   osclmemory \
   osclerror \
   osclbase

ifeq ($(pv2wayengine_lib),y)
LIBS = \
   pvomxvideodecnode \
   pvomxaudiodecnode \
   pvomxencnode \
   pvomxbasedecnode \
   omx_common_lib \
   omx_m4v_component_lib \
   omx_amr_component_lib \
   omx_amrenc_component_lib \
   omx_m4venc_component_lib \
   omx_avc_component_lib \
   omx_avcenc_component_lib \
   omx_baseclass_lib \
   m4v_config \
   pvomx_proxy_lib \
   omx_queue_lib \
   pv_config_parser \
   getactualaacconfig \
   osclregcli \
   osclregserv \
   threadsafe_callback_ao

FULL_LIBS := $(BASE_LIBS) + $(LIBS) + $(END_LIBS)

else
ifeq ($(pv2wayengine_lib),m)
FULL_LIBS =  opencore_2way \
   pvomxvideodecnode \
   pvomxaudiodecnode \
   pvomxencnode \
   pvomxbasedecnode \
   omx_common_lib \
   omx_m4v_component_lib \
   omx_m4venc_component_lib \
   omx_avc_component_lib \
   omx_avcenc_component_lib \
   omx_baseclass_lib \
   pvomx_proxy_lib \
   omx_queue_lib \
   unit_test_utils \
   unit_test \
   opencore_common 
	     
endif
endif

LIBS := $(FULL_LIBS)



include $(MK)/prog.mk


#look in 2way_$(HOST_ARCH).mk for values

run_2way_test_common:: $(REALTARGET) default
	$(quiet) ${RM} -r $(TWOWAY_TEST_DIR)
	$(quiet) ${MKDIR} -p $(TWOWAY_TEST_DIR)
	$(quiet) $(CP) $(SRC_ROOT)/tools_v2/build/package/opencore/elem/common/pvplayer.cfg $(TWOWAY_TEST_DIR)
	$(quiet) $(CP) $(SRC_ROOT)/engines/2way/test/src/pvlogcfg.txt $(TWOWAY_TEST_DIR)
	$(quiet) $(CP) ${BUILD_ROOT}/bin/${BUILD_ARCH}/$(TWOWAY_TARGET) $(TWOWAY_TEST_DIR)
	$(quiet) $(CP) -r $(SRC_ROOT)/engines/2way/sample_app/data/* $(TWOWAY_TEST_DIR)
#	$(quiet) run_2way_subtest
