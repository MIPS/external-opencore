# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pv2wayengine 


ifdef NO2WAYSIP
XCPPFLAGS += -DNO_2WAY_SIP
else
XCPPFLAGS += -DSIP_VOIP_PROJECT=1
endif


XCPPFLAGS += -DPV2WAY_USE_OMX 


XINCDIRS +=  ../../src  ../../include  ../../../common/include  \
../../../../protocols/systems/3g-324m_pvterminal/common/include \
../../../../protocols/systems/common/include  \
../../../../protocols/systems/tools/general/common/include  \


# OMX directories
XINCDIRS +=  ../../../../extern_libs_v2/khronos/openmax/include 


SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := pv_2way_datapath.cpp \
	pv_2way_engine.cpp \
	pv_2way_engine_test_ext.cpp \
	pv_2way_data_channel_datapath.cpp \
	pv_2way_cmd_control_datapath.cpp \
	pv_2way_dec_data_channel_datapath.cpp \
	pv_2way_enc_data_channel_datapath.cpp \
	pv_2way_mux_datapath.cpp \
	pv_2way_engine_factory.cpp \
	pv_2way_proxy_adapter.cpp \
	pv_2way_proxy_factory.cpp

HDRS := pv_2way_interface.h \
	pv_2way_engine_factory.h \
	pv_2way_test_extension_interface.h \
	pv_2way_proxy_factory.h 

## This file is need by PLATFORM_EXTRAS in library.mk
sdkinfo_header_name := pv_2way_sdkinfo.h

include $(MK)/library.mk

doc_target := pv2way_engine
doc_title := "PV2Way Engine"
doc_paths := "$(SRC_ROOT)/engines/2way/include $(SRC_ROOT)/protocols/systems/common/include/pv_2way_basic_types.h $(SRC_ROOT)/protocols/systems/3g-324m_pvterminal/common/include/pv_2way_h324m_types.h $(SRC_ROOT)/protocols/systems/3g-324m_pvterminal/h324/tsc/include/tsc_h324m_config_interface.h"
doc_version := $(PV2WAY_ENGINE_VERSION)

include $(MK)/doc.mk 

sdkinfo_target := $(LOCAL_SRCDIR)/pv_2way_engine.cpp
sdkinfo_header_filename := $(LOCAL_SRCDIR)/$(sdkinfo_header_name)
sdkinfo_header_macro := PV_2WAY_SDKINFO
sdkinfo_label_macro := PV2WAY_ENGINE_SDKINFO_LABEL
sdkinfo_date_macro := PV2WAY_ENGINE_SDKINFO_DATE

include $(MK)/sdkinfo.mk 
