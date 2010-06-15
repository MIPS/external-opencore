ifneq ($(pvplayreadyutility_lib),n)
XINCDIRS := ../../../../player/include ../../../../../extern_libs_v2/wmdrm/common/include ../../../../../extern_libs_v2/wmdrm/playready/include ../../../../../pvmi/content_policy_manager/plugins/common/include  ../../../../playready_utility/include ../../../../../pvmi/content_policy_manager/plugins/playready/include

ifeq ($(pvmetadata_engine_lib),m)
    XINCDIRS += ../../config/android

    LIBS := unit_test opencore_player opencore_common opencore_pvme pvwmdrmmd pvwmdrmoemsettings pvplayready
else
    XINCDIRS += ../../config/playready
    
    LIBS := $(LIBS_SET1) pvplayreadyutility $(LIBS_SET2)
endif
endif
