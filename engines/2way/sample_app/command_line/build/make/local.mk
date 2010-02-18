# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pv2way_console_app

ifdef twoway_config
include $(call process_include_list,$(LOCAL_PATH),$(twoway_config)).mk
endif

include $(LOCAL_PATH)/2way_$(HOST_ARCH).mk


XCXXFLAGS += $(FLAG_COMPILE_WARNINGS_AS_ERRORS)




XINCDIRS +=  ../../../src \
  ../../../../../../engines/2way/sample_app/include \
  ../../../../../../engines/2way/sample_app/pv2waysample/include \
  ../../../../../../engines/common/include \
  ../../../../../../protocols/systems/common/include \
  ../../../../test/include \
  ../../../../../../protocols/systems/tools/general/common/include 

SRCDIR := ../../src
INCSRCDIR := ../../include 

SRCS := main.cpp \
        pv_2way_console_source_and_sinks.cpp \
        console_engine_handler.cpp \
	../../pv2waysample/src/testcaseparser.cpp \
	../../../../../protocols/systems/tools/general/common/src/test_utility.cpp  

SRCS += $(EXTRA_SRCS)

BASE_LIBS = pv2waysample \
	pv2wayengine \
	pv324m 

END_LIBS = \
	pvclientserversocketnode \
	pvfileoutputnode \
	pvmediainputnode \
	pvmediaoutputnode \
	pvmiofileinput \
	pvmiofileoutput \
	pvmf \
	colorconvert \
	pvencoder_gsmamr \
	pvdecoder_gsmamr \
	pvmediadatastruct \
	pv_amr_nb_common_lib \
	pvamrwbdecoder \
	pvmp4decoder \
	pvm4vencoder \
        pvavcdecoder \
        pvavch264enc \
        pv_avc_common_lib \
	pvcommsionode \
	pvmio_comm_loopback \
	pvgeneraltools \
	pvthreadmessaging \
	pvmimeutils \
        pvlogger \
        osclio \
        unit_test \
        osclio \
	osclproc \
	osclutil \
	osclmemory \
	osclerror \
	osclbase

ifeq ($(pv2wayengine_lib),y)
# OMX libraries
LIBS =  \
   pvomxvideodecnode \
   pvomxaudiodecnode \
   pvomxencnode \
   pvomxbasedecnode \
   omx_common_lib \
   omx_avc_component_lib \
   omx_m4v_component_lib \
   omx_amr_component_lib \
   omx_amrenc_component_lib \
   omx_m4venc_component_lib \
   omx_avcenc_component_lib \
   omx_baseclass_lib \
   m4v_config \
   pvomx_proxy_lib \
   omx_queue_lib \
   pv_config_parser \
   getactualaacconfig \
   pvlatmpayloadparser \
   pvgendatastruct \
   osclregcli \
   osclregserv \
   threadsafe_callback_ao 

FULL_LIBS := $(BASE_LIBS) + $(LIBS) + $(END_LIBS)

else
ifeq ($(pv2wayengine_lib),m)
FULL_LIBS =  opencore_2way \
   pv2waysample \
   pvomxvideodecnode \
   pvomxaudiodecnode \
   pvomxencnode \
   pvomxbasedecnode \
   omx_common_lib \
   omx_avc_component_lib \
   omx_m4v_component_lib \
   omx_m4venc_component_lib \
   omx_avcenc_component_lib \
   omx_baseclass_lib \
   pvomx_proxy_lib \
   omx_queue_lib \
   unit_test \
   opencore_common

endif
endif

LIBS := $(FULL_LIBS)


SYSLIBS += $(SYS_THREAD_LIB)
SYSLIBS += $(SYS_SOCKET_LIB)

include $(MK)/prog.mk

 
#look in 2way_$(HOST_ARCH).mk for values
run_2way_console: $(REALTARGET) default
	$(quiet) ${RM} -rf $(TWOWAY_TEST_DIR_CON)
	$(quiet) ${MKDIR} -p $(TWOWAY_TEST_DIR_CON)
	$(quiet) $(CP) $(SRC_ROOT)/engines/2way/src/pvlogcfg.txt $(TWOWAY_TEST_DIR_CON)
	$(quiet) $(CP) ${BUILD_ROOT}/bin/${BUILD_ARCH}/$(TWOWAY_TARGET_CON) $(TWOWAY_TEST_DIR_CON)
	$(quiet) $(CP) -r $(SRC_ROOT)/engines/2way/sample_app/data/* $(TWOWAY_TEST_DIR_CON)
	$(quiet) cd $(TWOWAY_TEST_DIR_CON) && $(TWOWAYFULL_TARGET_CON) 

