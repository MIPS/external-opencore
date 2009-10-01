LOCAL_PATH := $(call get_makefile_dir)

include $(MK)/clear.mk

TARGET := pvme_test


XINCDIRS := ../../config/default ../../../../player/include

SRCDIR := ../../src
INCSRCDIR := ../../src

SRCS :=	test_pvme.cpp \
        test_pvme_testset1.cpp
        
ifeq ($(pvmetadata_engine_lib),m)
    LIBS := unit_test opencore_player opencore_common opencore_pvme
else
    LIBS := \
        pvmetadata_engine \
	pvmp3ffparsernode \
        pvmp4ffparsernode \
        cpm \
        pvaacffparsernode \
        pvwavffparsernode \
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
        pvmfrecognizer \
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
        pvplsffrecognizer
endif
	
SYSLIBS += $(SYS_THREAD_LIB)
SYSLIBS += $(SYS_SOCKET_LIB)

include $(MK)/prog.mk
