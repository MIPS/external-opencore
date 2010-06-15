ifneq ($(pvplayreadyutility_lib),n)
XINCDIRS := ../../../../../../../extern_libs_v2/khronos/openmax/include ../../../../../../../extern_libs_v2/wmdrm/common/include ../../../../../../../extern_libs_v2/wmdrm/playready/include ../../../../../../../pvmi/content_policy_manager/plugins/common/include  ../../../../../../playready_utility/include

XINCDIRS += ../../config/playready

LIBS += \
   pvplayreadyutility
endif