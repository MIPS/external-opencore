# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := test_omxenc_client

XINCDIRS += ../../../../../extern_libs_v2/khronos/openmax/include


SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := omx_threadsafe_callbacks.cpp \
	omxenctest.cpp \
	omxenctestbase.cpp \
	omxtest_buffer_busy_enc.cpp \
	omxtest_eosmissing_enc.cpp \
	omxtest_extra_partialframes_enc.cpp \
	omxtest_get_role_enc.cpp \
	omxtest_param_negotiation_enc.cpp \
	omxtest_partialframe_enc.cpp \
	omxtest_pause_resume_enc.cpp \
	omxtest_usebuffer_enc.cpp \
	omxtest_without_marker_enc.cpp



LIBS :=  		omx_common_lib \
			omx_m4venc_component_lib \
			omx_avcenc_component_lib \
			omx_amrenc_component_lib \
			omx_aacenc_component_lib \
			omx_aac_component_lib \
			omx_amr_component_lib \
			omx_mp3_component_lib \
			omx_wma_component_lib \
			omx_avc_component_lib \
			omx_m4v_component_lib \
			omx_wmv_component_lib \
			omx_ra_component_lib \
			omx_rv_component_lib \
			omx_baseclass_lib \
			pv_config_parser \
			getactualaacconfig \
			m4v_config \
			pvmp4decoder \
			pvm4vencoder \
			pvavcdecoder \
			pvavch264enc \
			pv_avc_common_lib \
			pvamrwbdecoder \
			pvdecoder_gsmamr \
			pvencoder_gsmamr \
			pv_amr_nb_common_lib \
			rvdecoder \
			pvra8decoder \
			pvrmffparser \
			pv_aac_enc \
			pv_aac_dec \
			pvmp3 \
			wmadecoder \
			wmavoicedecoder \
			wmvdecoder \
			colorconvert \
			unit_test \
			pvomx_proxy_lib \
			omx_queue_lib \
			threadsafe_callback_ao \
			pvlogger \
			osclio \
			osclproc \
			osclutil \
			osclmemory \
			osclerror \
			osclbase
           

SYSLIBS += $(SYS_THREAD_LIB)

# If user has not specified any test arguments
ifeq ($(TEST_ARGS),)
  
# Run AMR test case number 2 as the default run target
TEST_ARGS := -config $(SRC_ROOT)/codecs_v2/omx/omx_testapp_enc/data/amrenc_test_omx.txt -c amr -t 2 2

#Enable the flag to run all other formats
RUN_ALL_TARGETS:=1

endif

include $(MK)/prog.mk

ifeq ($(RUN_ALL_TARGETS),1)

run_omx_avcenc_test: REALTARGET:=$(REALTARGET)
run_omx_avcenc_test: CW_DIR := $(LOCAL_PATH)

run_omx_avcenc_test: $(REALTARGET)
	$(call cd_and_run_test,$(CW_DIR),$(REALTARGET),-config $(SRC_ROOT)/codecs_v2/omx/omx_testapp_enc/data/avcenc_test_omx.txt -c avc)

run_omx_mpeg4enc_test: REALTARGET:=$(REALTARGET)
run_omx_mpeg4enc_test: CW_DIR := $(LOCAL_PATH)
run_omx_mpeg4enc_test: $(REALTARGET)
	$(call cd_and_run_test,$(CW_DIR),$(REALTARGET),-config $(SRC_ROOT)/codecs_v2/omx/omx_testapp_enc/data/m4venc_test_omx.txt -c mpeg4)

run_omx_aacenc_test: REALTARGET:=$(REALTARGET)
run_omx_aacenc_test: CW_DIR := $(LOCAL_PATH)

run_omx_aacenc_test: $(REALTARGET)
	$(call cd_and_run_test,$(CW_DIR),$(REALTARGET),-config $(SRC_ROOT)/codecs_v2/omx/omx_testapp_enc/data/aacenc_test_omx.txt -c aac)
	
run_omx_amrenc_test: REALTARGET:=$(REALTARGET)
run_omx_amrenc_test: CW_DIR := $(LOCAL_PATH)

run_omx_amrenc_test: $(REALTARGET)
	$(call cd_and_run_test,$(CW_DIR),$(REALTARGET),-config $(SRC_ROOT)/codecs_v2/omx/omx_testapp_enc/data/amrenc_test_omx.txt -c amr)

run_test_omxenc_client : run_omx_avcenc_test run_omx_mpeg4enc_test run_omx_aacenc_test run_omx_amrenc_test

endif
