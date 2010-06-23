LOCAL_PATH := $(call get_makefile_dir)

include $(MK)/clear.mk

TARGET := pvme_test

XINCDIRS := ../../../../player/include ../../../../../extern_libs_v2/wmdrm/common/include ../../../../../extern_libs_v2/wmdrm/playready/include ../../../../../pvmi/content_policy_manager/plugins/common/include  ../../../../playready_utility/include ../../../../../pvmi/content_policy_manager/plugins/playready/include

SRCDIR := ../../src
INCSRCDIR := ../../src

SRCS :=	test_pvme.cpp \
        test_pvme_testset1.cpp \
        test_pvme_testset_wmdrmcpm.cpp
        
ifeq ($(pvmetadata_engine_lib),m)
    XINCDIRS += ../../config/android
    
#   test_pvme_testset_wmdrmcpm.cpp has bee included in include_paths_extended 
    SRCS := test_pvme.cpp \
            test_pvme_testset1.cpp 

    LIBS := unit_test opencore_player opencore_common opencore_pvme pvwmdrmmd pvwmdrmoemsettings pvplayready
else
    XINCDIRS += ../../config/default
    
    LIBS_SET1 := \
        pvmetadata_engine \
	pvmp3ffparsernode \
        pvmp4ffparsernode \
        cpm \
        pvaacffparsernode \
        pvwavffparsernode \
        pv_smooth_streaming \
        pvtinyxml \
        pvmp4ffcomposer \
        pvmp4ff \
        pvmp3ff \
        pvwav \
        pvaacparser \
        getactualaacconfig \
        m4v_config \
        pvmf \
        pv_config_parser \
        pvmp3 \
        pvmimeutils \
        pvmediadatastruct \
        pvamrffparsernode \
        pvgsmamrparser \
        pvasfffparsernode \
        pvasfff \
        pvplayreadyplugin

    LIBS_SET2 := pvmfrecognizer \
        pvasfffrecognizer \
        pvmp4ffrecognizer \
        mp4recognizer_utility \
        pvmp3ffrecognizer \
        pvrmffrecognizer \
        pvoma1ffrecognizer \
	pvaacffrecognizer \
	pvwavffrecognizer \
        pvfileparserutils \
        pvrmffparsernode \
        pvrmffparser \
        pvrmff \
        pvrmffrecognizer_utility \
        pvid3parcom \
        pvgendatastruct \
        pvwmdrmmdlib \
        csprng \
        pvcrypto \
        pventropysrc \
        pvlogger \
        osclregcli \
        osclregserv \
        osclio \
        osclproc \
        osclutil \
        osclmemory \
        osclerror \
        osclbase \
        threadsafe_callback_ao \
        unit_test \
        asfrecognizer_utility \
        scsp \
        pvdivxffrecognizer \
        pvdivxffparsernode \
        pv_divxfile_parser \
        pvamrffrecognizer \
        pvaacffrecognizer \
        divxrecognizer_utility \
        pvplsffrecognizer \
	pvcommonparsernode \
	wavparser \
	amrparser \
	audioparser
# playreadyutility will be linked between these two sets of libs in case it is selected
    LIBS := $(LIBS_SET1) $(LIBS_SET2)
endif
	
SYSLIBS += $(SYS_THREAD_LIB)
SYSLIBS += $(SYS_SOCKET_LIB)

-include $(LOCAL_PATH)/playready.mk
include $(MK)/prog.mk
