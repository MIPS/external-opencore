# $(info within android.mk)

############################################################################
#
#  General rules to make and clean
#



############################################################################
############################################################################

# macro to define printf.  This macro needs to be able to handle
# backslash-escaped characters such as \t(tab), \n(newline),etc.
# "echo -e" handles this but certain shells such as dash (on ubuntu)
# don't handle the -e option in their builtin echo implementation.
# However, the printf builtin of these sh shells should work.
PRINTF:=printf

############################################################################
esc_dollar := \$$


############################################################################
# Original method using multiple echo statements separated by ';', but this 
# seemed much less efficient then putting it into one statement.  
# See second version below.
#define output_list
#$(foreach elem,$(call truncate,$1),$(PRINTF) "\t$(elem) \\" >> $2;) $(PRINTF) "\t$(lastword $1)\\n" >> $2
#endef



define output_list
  $(PRINTF) "$(foreach elem,$(call truncate,$1),\\t$(elem) \\\\\n) \\t$(lastword $1)\\n" >> $2
endef

define output_lib_list
  $(PRINTF) "$(foreach elem,$(call truncate,$1),\\tlib$(elem) \\\\\n) \\tlib$(lastword $1)\\n" >> $2
endef

define format_shared_lib_names
  $(subst -l,lib,$1)
endef

define convert_component_lib_makefile_name
  $(patsubst $(SRC_ROOT)/%,\$$(PV_TOP)/%,$1)
endef

define output_include_list
  $(PRINTF) "$(subst $(SPACE)include,include,$(foreach elem,$(call truncate,$(strip $1)),include $(call convert_component_lib_makefile_name,$(elem))\\n))include $(call convert_component_lib_makefile_name,$(lastword $(strip $1)))\\n" >> $2  
endef

define include_staticlibs_list
  $(if $(strip $(call remove_quotes,$1)),$(PRINTF) "$(foreach elem,$(strip $(call remove_quotes,$1)),include $(patsubst %,%/Android.mk,$(patsubst %,\$$(PV_TOP)%,$(call strip_two_levels_up,$(elem)/local.mk)))\n)" >> $2,)
endef

define output_assembly_srcs
  $(if $(strip $1),$(PRINTF) "ifeq (\$$(TARGET_ARCH),arm)\\nLOCAL_SRC_FILES += \\\\\n$(foreach elem,$(call truncate,$1),\\t$(elem) \\\\\n)\\t$(lastword $1)\\nendif\\n\\n" >> $2,)
endef

define extra_lib_list
  $(if $(strip $1),$(PRINTF) "\nLOCAL_WHOLE_STATIC_LIBRARIES += $1\n" >> $2,)
endef

define extra_sharedlib_list
  $(if $(strip $1),$(PRINTF) "\nLOCAL_SHARED_LIBRARIES := $1\n" >> $2,)
endef

define conditional_extra_sharedlib_list
  $(if $(strip $1),$(PRINTF) "    LOCAL_SHARED_LIBRARIES := $1\n" >> $2,)
endef

define extra_include_list
  $(if $(strip $1),$(PRINTF) "$(foreach elem, $1,include $(patsubst %,%/Android.mk,$(patsubst %,\$$(PV_TOP)%,$(strip $(elem))))\n)" >> $2,)
endef

define is_prelinking_allowed
  $(if $(strip $1),,$(PRINTF) "\nLOCAL_PRELINK_MODULE := false\n" >> $2)
endef

define cfg_list
  $(if $(strip $1),$(PRINTF) "\$$(call add-prebuilt-files, ETC, $1)\ninclude \$$(CLEAR_VARS)\n" >> $2)
endef

############################################################################


include $(MK)/android_segments.mk

#############################################
#    Rules for aggregate makefiles
#
ifneq ($(AGGREGATE_LIBS_MAKEFILE),)

#### Start generation of aggregate makefiles #######

define create_sdk_specific_aggregate_lib_android_mk
Android_$1.mk: FORCE
	$$(quiet) echo "LOCAL_PATH := $$(esc_dollar)(call my-dir)" > $$@
	$$(quiet) echo "include $$(esc_dollar)(CLEAR_VARS)" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) $$(call cfg_list, $$(CFG_$1),$$@)
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "LOCAL_WHOLE_STATIC_LIBRARIES := \\" >> $$@
	$$(quiet) $$(call output_lib_list,$$($1_CUMULATIVE_TARGET_LIST),$$@)
	$$(quiet) $$(call extra_lib_list, $$(EXTRA_LIBS_$1),$$@)
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "LOCAL_MODULE := lib$1" >> $$@
	$$(quiet) $$(call is_prelinking_allowed,$$($1_PRELINK),$$@)
	$$(quiet) echo "ifeq ($$(esc_dollar)(PLATFORM_VERSION),1.5)" >> $$@
	$$(quiet) $$(call conditional_extra_sharedlib_list, $$(EXTRA_SHARED_LIBRARIES_$1_1.5),$$@)
	$$(quiet) echo "else ifeq ($$(esc_dollar)(PLATFORM_VERSION),1.6)" >> $$@
	$$(quiet) $$(call conditional_extra_sharedlib_list, $$(EXTRA_SHARED_LIBRARIES_$1_1.6),$$@)
	$$(quiet) echo "else ifeq ($$(esc_dollar)(PLATFORM_VERSION),2.1)" >> $$@
	$$(quiet) $$(call conditional_extra_sharedlib_list, $$(EXTRA_SHARED_LIBRARIES_$1_2.1),$$@)
	$$(quiet) echo "else" >> $$@
	$$(quiet) $$(call conditional_extra_sharedlib_list, $$(EXTRA_SHARED_LIBRARIES_$1),$$@)
	$$(quiet) echo "endif" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "-include $$(esc_dollar)(PV_TOP)/Android_system_extras.mk" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "LOCAL_SHARED_LIBRARIES += $$(call format_shared_lib_names,$$(MODS_$1))" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "include $$(esc_dollar)(BUILD_SHARED_LIBRARY)" >> $$@
	$$(quiet) $$(call output_include_list,$$($1_CUMULATIVE_MAKEFILES),$$@)
	$$(quiet) $$(call extra_include_list, $$(EXTRA_MAKEFILES_PATHS_$1),$$@)
	$$(quiet) echo "" >> $$@
endef

define create_aggregate_lib_android_mk
Android_$1.mk: FORCE
	$$(quiet) echo "LOCAL_PATH := $$(esc_dollar)(call my-dir)" > $$@
	$$(quiet) echo "include $$(esc_dollar)(CLEAR_VARS)" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) $$(call cfg_list, $$(CFG_$1),$$@)
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "LOCAL_WHOLE_STATIC_LIBRARIES := \\" >> $$@
	$$(quiet) $$(call output_lib_list,$$($1_CUMULATIVE_TARGET_LIST),$$@)
	$$(quiet) $$(call extra_lib_list, $$(EXTRA_LIBS_$1),$$@)
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "LOCAL_MODULE := lib$1" >> $$@
	$$(quiet) $$(call is_prelinking_allowed,$$($1_PRELINK),$$@)
	$$(quiet) $$(call extra_sharedlib_list, $$(EXTRA_SHARED_LIBRARIES_$1),$$@)
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "-include $$(esc_dollar)(PV_TOP)/Android_system_extras.mk" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "LOCAL_SHARED_LIBRARIES += $$(call format_shared_lib_names,$$(MODS_$1))" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "include $$(esc_dollar)(BUILD_SHARED_LIBRARY)" >> $$@
	$$(quiet) $$(call output_include_list,$$($1_CUMULATIVE_MAKEFILES),$$@)
	$$(quiet) $$(call extra_include_list, $$(EXTRA_MAKEFILES_PATHS_$1),$$@)
	$$(quiet) echo "" >> $$@
endef

#### End generation of aggregate makefiles #######



#### Start generation of top level makefile #######

define include_module_mk_list
 $(PRINTF) "$(subst $(SPACE)include,include,$(foreach elem,$1,include $(patsubst $(SRC_ROOT)/%,\$$(PV_TOP)/%,$(CFG_DIR))/Android_$(elem).mk\n))" >> $2
endef

define include_test_mk_list
 $(PRINTF) "$(subst $(SPACE)include,include,$(foreach app,$(strip $(call remove_quotes,$1)),include \$$(PV_TOP)$(strip $(call strip_two_levels_up,$(call remove_quotes,$(TESTAPP_DIR_$(app))/local.mk)))/Android.mk\n))" >> $2
endef

define include_extended_features_mk
  $(PRINTF) "\055include $(esc_dollar)(PV_TOP)/extern_libs_v2/android/extended_features/Android.mk\n" >> $1
endef

define create_toplevel_android_mk
$1: FORCE
	$$(quiet) echo "ifneq ($$(esc_dollar)(BUILD_WITHOUT_PV),true)" > $$@
	$$(quiet) echo "LOCAL_PATH := $$(esc_dollar)(call my-dir)" >> $$@
	$$(quiet) echo "include $$(esc_dollar)(CLEAR_VARS)" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "# Set up the PV variables" >> $$@
	$$(quiet) echo "include $$(esc_dollar)(LOCAL_PATH)/Config.mk" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) $$(call include_module_mk_list,$2,$$@)
	$$(quiet) echo "ifeq ($$(esc_dollar)(BUILD_PV_2WAY),1)" >> $$@
	$$(quiet) $$(call include_module_mk_list,$3,$$@)
	$$(quiet) echo "endif" >> $$@
	$$(quiet) $$(call include_staticlibs_list,$$(LIBDIR_static),$$@)
	$$(quiet) echo "ifeq ($$(esc_dollar)(BUILD_PV_TEST_APPS),1)" >> $$@
	$$(quiet) $$(call include_test_mk_list,$$(TESTAPPS_WO_2WAY),$$@)
	$$(quiet) echo "ifeq ($$(esc_dollar)(BUILD_PV_2WAY),1)" >> $$@
	$$(quiet) $$(call include_test_mk_list,$$(2WAY_TESTAPP),$$@)
	$$(quiet) echo "endif" >> $$@
	$$(quiet) echo "endif" >> $$@
	$$(quiet) $$(call include_extended_features_mk,$$@)
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "endif" >> $$@
endef

BUILD_CFG_DIR := $(patsubst $(SRC_ROOT)/%,(PV_TOP)/%,$(CFG_DIR))

define create_opencore_config_mk
$1: FORCE
	$$(quiet) echo "ifneq ($$(esc_dollar)(strip $$(esc_dollar)(EXTERNAL_OPENCORE_CONFIG_ONCE)),true)" > $$@
	$$(quiet) echo "  # This is the first attempt to include this file" >> $$@
	$$(quiet) echo "  EXTERNAL_OPENCORE_CONFIG_ONCE := true" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "  PV_TOP := $$(esc_dollar)(my-dir)" >> $$@
	$$(quiet) echo "" >> $$@

	$$(quiet) echo "  PV_CFLAGS := -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DUSE_CML2_CONFIG" >> $$@
	$$(quiet) echo "  ifeq ($$(esc_dollar)(PLATFORM_VERSION),1.5)" >> $$@
	$$(quiet) echo "  else ifeq ($$(esc_dollar)(PLATFORM_VERSION),1.6)" >> $$@
	$$(quiet) echo "  else ifeq ($$(esc_dollar)(PLATFORM_VERSION),2.1)" >> $$@
	$$(quiet) echo "    ifeq ($$(esc_dollar)(PV_WERROR),1)" >> $$@
	$$(quiet) echo "      PV_CFLAGS += -Wno-psabi" >> $$@
	$$(quiet) echo "    endif" >> $$@
	$$(quiet) echo "  else" >> $$@
	$$(quiet) echo "    ifeq ($$(esc_dollar)(PV_WERROR),1)" >> $$@
	$$(quiet) echo "      PV_CFLAGS += -Wno-psabi" >> $$@
	$$(quiet) echo "    endif" >> $$@
	$$(quiet) echo "  endif" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "  ifeq ($$(esc_dollar)(PV_WERROR),1)" >> $$@
	$$(quiet) echo "    PV_CFLAGS += -Werror" >> $$@
	$$(quiet) echo "  endif" >> $$@
	$$(quiet) echo "  ifeq ($$(esc_dollar)(TARGET_ARCH),arm)" >> $$@
	$$(quiet) echo "    ifeq ($(TARGET_ARCH_VERSION),armv4t)" >> $$@
	$$(quiet) echo "      PV_CFLAGS += -DPV_ARM_GCC_V4" >> $$@
	$$(quiet) echo "    else" >> $$@
	$$(quiet) echo "      PV_CFLAGS += -DPV_ARM_GCC_V5" >> $$@
	$$(quiet) echo "    endif" >> $$@
	$$(quiet) echo "  endif"  >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "  FORMAT := android" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "  PV_COPY_HEADERS_TO := libpv" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "  PV_CFLAGS_MINUS_VISIBILITY := $$(esc_dollar)(PV_CFLAGS)" >> $$@
	$$(quiet) echo "  PV_CFLAGS += -fvisibility=hidden" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "  PV_INCLUDES := \\" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/android \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/../sqlite/dist \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/../../frameworks/base/core/jni \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(JNI_H_INCLUDE) \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/extern_libs_v2/khronos/openmax/include \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/engines/common/include \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/engines/player/config/android \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/engines/player/include \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/nodes/pvmediaoutputnode/include \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/nodes/pvdownloadmanagernode/config/opencore \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/pvmi/pvmf/include \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/fileformats/mp4/parser/config/opencore \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/oscl/oscl/config/android \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/oscl/oscl/config/shared \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/engines/author/include \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(PV_TOP)/android/drm/oma1/src \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)$(BUILD_CFG_DIR) \\\\\n" >> $$@
	$$(quiet) $(PRINTF) "\t$$(esc_dollar)(TARGET_OUT_HEADERS)/$$(esc_dollar)(PV_COPY_HEADERS_TO)" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "" >> $$@
	$$(quiet) echo "  # Stash these values for the next include of this file" >> $$@
	$$(quiet) echo "  OPENCORE.PV_TOP := $$(esc_dollar)(PV_TOP)" >> $$@
	$$(quiet) echo "  OPENCORE.PV_CFLAGS := $$(esc_dollar)(PV_CFLAGS)" >> $$@
	$$(quiet) echo "  OPENCORE.PV_CFLAGS_MINUS_VISIBILITY := $$(esc_dollar)(PV_CFLAGS_MINUS_VISIBILITY)" >> $$@
	$$(quiet) echo "  OPENCORE.FORMAT := $$(esc_dollar)(FORMAT)" >> $$@
	$$(quiet) echo "  OPENCORE.PV_COPY_HEADERS_TO := $$(esc_dollar)(PV_COPY_HEADERS_TO)" >> $$@
	$$(quiet) echo "  OPENCORE.PV_INCLUDES := $$(esc_dollar)(PV_INCLUDES)" >> $$@
	$$(quiet) echo "else" >> $$@
	$$(quiet) echo "  # This file has already been included by someone, so we can" >> $$@
	$$(quiet) echo "  # use the precomputed values." >> $$@
	$$(quiet) echo "  PV_TOP := $$(esc_dollar)(OPENCORE.PV_TOP)" >> $$@
	$$(quiet) echo "  PV_CFLAGS := $$(esc_dollar)(OPENCORE.PV_CFLAGS)" >> $$@
	$$(quiet) echo "  PV_CFLAGS := $$(esc_dollar)(OPENCORE.PV_CFLAGS_MINUS_VISIBILITY)" >> $$@
	$$(quiet) echo "  FORMAT := $$(esc_dollar)(OPENCORE.FORMAT)" >> $$@
	$$(quiet) echo "  PV_COPY_HEADERS_TO := $$(esc_dollar)(OPENCORE.PV_COPY_HEADERS_TO)" >> $$@
	$$(quiet) echo "  PV_INCLUDES := $$(esc_dollar)(OPENCORE.PV_INCLUDES)" >> $$@
	$$(quiet) echo "endif" >> $$@
endef
#### End generation of top level makefile #######


# loop over all the names in AGGREGATE_LIB_TARGET_LIST and 
#  create the Android makefile name list.
#  Append top-level Android.mk
ANDROID_TOPLEVEL_MAKE_NAME := Android.mk
OPENCORE_CONFIG_MAKE_NAME := Config.mk

# The ANDROID_AGGREGATE_LIB_LIST is built from AGGREGATE_LIB_TARGET_LIST by stripping out those aggregate libraries that end 
# up being empty in the Android build.  This typically happens because the library as part of the platform for Android 
# so we don't need to build our own version.
ANDROID_AGGREGATE_LIB_LIST :=
$(strip $(foreach lib,$(AGGREGATE_LIB_TARGET_LIST),$(if $(strip $($(lib)_CUMULATIVE_TARGET_LIST) $(EXTRA_LIBS_$(lib))),$(eval ANDROID_AGGREGATE_LIB_LIST += $(lib)),)))
ANDROID_MAKE_NAMES := $(patsubst %,Android_%.mk,$(ANDROID_AGGREGATE_LIB_LIST)) $(ANDROID_TOPLEVEL_MAKE_NAME) $(OPENCORE_CONFIG_MAKE_NAME)

# player, author, wmdrm, wmdrmservice, and mtpservice need SDK-specific EXTRA_SHARED_LIBRARIES
AUTHOR_SHARED_LIB := opencore_author
PLAYER_SHARED_LIB := opencore_player
WMDRM_SHARED_LIB := pvwmdrm
WMDRMSERVICE_SHARED_LIB := pvwmdrmservice
WMDRMOEMSETTINGS_SHARED_LIB := pvwmdrmoemsettings
MTPSERVICE_SHARED_LIB := pvmtpservice
ANDROID_GENERAL_AGGREGATE_LIB_LIST := $(strip $(subst $(AUTHOR_SHARED_LIB),,$(ANDROID_AGGREGATE_LIB_LIST)))
ANDROID_GENERAL_AGGREGATE_LIB_LIST := $(strip $(subst $(PLAYER_SHARED_LIB),,$(ANDROID_GENERAL_AGGREGATE_LIB_LIST)))
ANDROID_GENERAL_AGGREGATE_LIB_LIST := $(strip $(subst $(WMDRM_SHARED_LIB),,$(ANDROID_GENERAL_AGGREGATE_LIB_LIST)))
ANDROID_GENERAL_AGGREGATE_LIB_LIST := $(strip $(subst $(WMDRMSERVICE_SHARED_LIB),,$(ANDROID_GENERAL_AGGREGATE_LIB_LIST)))
ANDROID_GENERAL_AGGREGATE_LIB_LIST := $(strip $(subst $(WMDRMOEMSETTINGS_SHARED_LIB),,$(ANDROID_GENERAL_AGGREGATE_LIB_LIST)))
ANDROID_GENERAL_AGGREGATE_LIB_LIST := $(strip $(subst $(MTPSERVICE_SHARED_LIB),,$(ANDROID_GENERAL_AGGREGATE_LIB_LIST)))
$(eval $(call create_sdk_specific_aggregate_lib_android_mk,$(AUTHOR_SHARED_LIB)))
$(eval $(call create_sdk_specific_aggregate_lib_android_mk,$(PLAYER_SHARED_LIB)))
$(eval $(call create_sdk_specific_aggregate_lib_android_mk,$(WMDRM_SHARED_LIB)))
$(eval $(call create_sdk_specific_aggregate_lib_android_mk,$(WMDRMSERVICE_SHARED_LIB)))
$(eval $(call create_sdk_specific_aggregate_lib_android_mk,$(WMDRMOEMSETTINGS_SHARED_LIB)))
$(eval $(call create_sdk_specific_aggregate_lib_android_mk,$(MTPSERVICE_SHARED_LIB)))
$(strip $(foreach lib,$(ANDROID_GENERAL_AGGREGATE_LIB_LIST),$(eval $(call create_aggregate_lib_android_mk,$(lib)))))

# Need the ability exclude 2way by default
2WAY_SHARED_LIB := opencore_2way
AGGREGATE_LIB_TARGET_LIST_WO_2WAY := $(strip $(subst $(2WAY_SHARED_LIB),,$(ANDROID_AGGREGATE_LIB_LIST)))
2WAY_TESTAPP := pv2way_engine_test
TESTAPPS_WO_2WAY := $(strip $(subst $(2WAY_TESTAPP),,$(TESTAPPS)))

$(eval $(call create_toplevel_android_mk,$(ANDROID_TOPLEVEL_MAKE_NAME),$(AGGREGATE_LIB_TARGET_LIST_WO_2WAY),$(2WAY_SHARED_LIB)))
$(eval $(call create_opencore_config_mk,$(OPENCORE_CONFIG_MAKE_NAME)))


android_clean: modulelevel_android_mk_clean toplevel_android_mk_clean opencore_config_mk_clean

modulelevel_android_mk_clean: ANDROID_MAKE_FILES_TO_CLEAN := $(ANDROID_MAKE_NAMES) 
toplevel_android_mk_clean: ANDROID_MAKE_FILES_TO_CLEAN += $(ANDROID_TOPLEVEL_MAKE_NAME)
opencore_config_mk_clean: ANDROID_MAKE_FILES_TO_CLEAN += $(OPENCORE_CONFIG_MAKE_NAME)

modulelevel_android_mk_clean: FORCE
	$(quiet) $(RM) $(ANDROID_MAKE_FILES_TO_CLEAN)

toplevel_android_mk_clean: FORCE
	$(quiet) $(RM) $(ANDROID_TOPLEVEL_MAKE_NAME)

opencore_config_mk_clean: FORCE
	$(quiet) $(RM) $(OPENCORE_CONFIG_MAKE_NAME)
#############################################
#    Rules for a single library makefile
#
else

define check_solib_plugins
$(if $(strip $1),$(if $(strip $(filter opencore,$(subst _, ,$2))),,$1),)
endef


define include_system_extras
  $(if $(strip $(filter $1,BUILD_EXECUTABLE)),$(PRINTF) "\n-include \$$(PV_TOP)/Android_system_extras.mk\n" >> $2,)
endef

define include_local_c_flags
  $(if $(strip $(1)),$(PRINTF) "LOCAL_CFLAGS := $(ANDROID_C_FLAGS) \$$(PV_CFLAGS_MINUS_VISIBILITY)" >> $2, $(PRINTF) "LOCAL_CFLAGS := $(ANDROID_C_FLAGS) \$$(PV_CFLAGS)" >> $2)
endef

define undo_warnings_as_errors
  $(if $(ANDROID_C_FLAGS_X), $(PRINTF) "\nLOCAL_CFLAGS += $(ANDROID_C_FLAGS_X)" >> $1)
endef

ifeq ($(LOCAL_DISABLE_COMPILE_WARNINGS_AS_ERRORS),1)
  $(TARGET)_DISABLE_COMPILE_WARNINGS_AS_ERRORS = -Wno-error
endif

ifeq ($(LOCAL_ANDROID_MK_PATH),)
  LOCAL_ANDROID_MK_PATH := $(strip $(call strip_two_levels_up,$(strip $(LOCAL_PATH)/local.mk)))
endif


# $(warning ***** LOCAL_ANDROID_MK_PATH = $(LOCAL_ANDROID_MK_PATH))


ifneq ($(strip $(call check_solib_plugins,$($(TARGET)_plugins_$(AGGREGATE_LIB)),$(AGGREGATE_LIB))),)
# These rules are to handle cases where we build the same sources into
# different libraries with the different compile flags.  This support is 
# hopefully temporary as the sources should really be refactored so the 
# common part is separated and does not need to be built into multiple libs.

ANDROID_MAKE_NAMES := $(LOCAL_ANDROID_MK_PATH)/Android$(AGGREGATE_LIB_TARGET_COMP).mk
ANDROID_TMP_LOCAL_SRCDIR := $(patsubst %src,%src$(AGGREGATE_LIB_TARGET_COMP),$(LOCAL_SRCDIR))
ANDROID_TMP_SRCDIR := $(patsubst %src,%src$(AGGREGATE_LIB_TARGET_COMP),$(SRCDIR))

ANDROID_TMP_LOCAL_INC := $(subst $(LOCAL_SRCDIR),$(ANDROID_TMP_LOCAL_SRCDIR),$(LOCAL_TOTAL_INCDIRS))
ANDROID_TMP_LOCAL_INC := $(subst $(SRC_ROOT),\$$(PV_TOP),$(ANDROID_TMP_LOCAL_INC)) \$$(PV_INCLUDES)
ANDROID_TMP_ASMDIRS :=   $(subst $(LOCAL_SRCDIR),$(ANDROID_TMP_LOCAL_SRCDIR),$(LOCAL_ASM_INCDIRS))
ANDROID_TMP_ASMDIRS := $(subst $(SRC_ROOT),\$$(PV_TOP),$(ANDROID_TMP_ASMDIRS))

ANDROID_TMP_TARGET := $(TARGET)$(AGGREGATE_LIB_TARGET_COMP)

ifeq ($($(TARGET)_libtype),shared-archive)
# strip the last word from target and add the new android target
CUMULATIVE_TARGET_LIST := $(call truncate,$(CUMULATIVE_TARGET_LIST)) $(ANDROID_TMP_TARGET)
# must also create a new "fullname" variable because the CML2 template 
# will use the values in the CUMULATIVE_TARGET_LIST to map to the 
# "fullname" variables which hold the corresponding library path
$(ANDROID_TMP_TARGET)$(AGGREGATE_LIB_TARGET_COMP)_fullname := $(LIBTARGET)
endif

$(ANDROID_MAKE_NAMES): $(ANDROID_TMP_LOCAL_SRCDIR)
# $(warning defining copy srcdir for $(ANDROID_TMP_LOCAL_SRCDIR))


$(ANDROID_TMP_LOCAL_SRCDIR): ANDROID_SRCDIR_TO_COPY := $(LOCAL_SRCDIR)
$(ANDROID_TMP_LOCAL_SRCDIR): FORCE
	echo "Copying $(ANDROID_SRCDIR_TO_COPY) to $@"
	$(quiet) $(CP) -r $(ANDROID_SRCDIR_TO_COPY) $@

android_clean: $(ANDROID_TMP_LOCAL_SRCDIR)_android_srcdir_clean $(ANDROID_TMP_LOCAL_SRCDIR)_android_mk_clean

$(ANDROID_TMP_LOCAL_SRCDIR)_android_srcdir_clean: ANDROID_DIRS_TO_CLEAN := $(ANDROID_TMP_LOCAL_SRCDIR)

$(ANDROID_TMP_LOCAL_SRCDIR)_android_srcdir_clean: FORCE
	@echo "Cleaning dir $(ANDROID_DIRS_TO_CLEAN)"
	$(quiet) $(RMDIR) $(ANDROID_DIRS_TO_CLEAN)

$(ANDROID_TMP_LOCAL_SRCDIR)_android_mk_clean: ANDROID_MAKE_FILES_TO_CLEAN := $(ANDROID_MAKE_NAMES)

$(ANDROID_TMP_LOCAL_SRCDIR)_android_mk_clean: FORCE
	$(quiet) $(RM) $(ANDROID_MAKE_FILES_TO_CLEAN)


else

ANDROID_MAKE_NAMES := $(LOCAL_ANDROID_MK_PATH)/Android.mk
ANDROID_TMP_LOCAL_SRCDIR := $(LOCAL_SRCDIR)
ANDROID_TMP_SRCDIR := $(SRCDIR)
ANDROID_TMP_LOCAL_INC := $(subst $(SRC_ROOT),\$$(PV_TOP),$(LOCAL_TOTAL_INCDIRS)) \$$(PV_INCLUDES)
ANDROID_TMP_ASMDIRS := $(subst $(SRC_ROOT),\$$(PV_TOP),$(LOCAL_ASM_INCDIRS))
ANDROID_TMP_TARGET := $(TARGET)

android_clean: $(LOCAL_PATH)_android_mk_clean

$(LOCAL_PATH)_android_mk_clean: ANDROID_MAKE_FILES_TO_CLEAN := $(ANDROID_MAKE_NAMES)

$(LOCAL_PATH)_android_mk_clean: FORCE
	$(quiet) $(RM) $(ANDROID_MAKE_FILES_TO_CLEAN)

endif
ANDROID_TMP_C_FLAGS := $(subst $(SRC_ROOT),\$$(PV_TOP),$(XCPPFLAGS))

# $(warning ***** ANDROID_MAKE_NAMES = $(ANDROID_MAKE_NAMES))

CUMULATIVE_MAKEFILES := $(CUMULATIVE_MAKEFILES) $(ANDROID_MAKE_NAMES)

AND_LOCAL_ARM_MODE := $(if $(filter $(strip $(OPTIMIZE_FOR_PERFORMANCE_OVER_SIZE)),true),LOCAL_ARM_MODE := arm,)
AND_LOCAL_EXPORT_ALL_SYMBOLS := $(LOCAL_EXPORT_ALL_SYMBOLS)

$(ANDROID_MAKE_NAMES): ANDROID_CPP_SRCS := $(if $(strip $(SRCS)),$(patsubst %,$(call go_up_two_levels,$(ANDROID_TMP_SRCDIR))/%,$(filter %.cpp,$(SRCS))),)
$(ANDROID_MAKE_NAMES): ANDROID_ASM_SRCS := $(if $(strip $(SRCS)),$(patsubst %,$(call go_up_two_levels,$(ANDROID_TMP_SRCDIR))/%,$(filter-out %.cpp,$(SRCS))),)
$(ANDROID_MAKE_NAMES): ANDROID_TARGET := $(if $(strip $(filter prog,$(TARGET_TYPE))),"LOCAL_MODULE :=" $(TARGET),$(if $(strip $(TARGET)),"LOCAL_MODULE :=" lib$(ANDROID_TMP_TARGET),))
$(ANDROID_MAKE_NAMES): ANDROID_HDRS := $(patsubst %,$(call go_up_two_levels,$(INCSRCDIR))/%,$(HDRS))
$(ANDROID_MAKE_NAMES): ANDROID_C_FLAGS := $(filter-out %PV_ARM_GCC_V5,$(ANDROID_TMP_C_FLAGS)) $(ANDROID_TMP_ASMDIRS)
$(ANDROID_MAKE_NAMES): ANDROID_C_FLAGS_X := $($(TARGET)_DISABLE_COMPILE_WARNINGS_AS_ERRORS)
$(ANDROID_MAKE_NAMES): ANDROID_C_INC := $(ANDROID_TMP_LOCAL_INC)
$(ANDROID_MAKE_NAMES): ANDROID_ARM_MODE := $(AND_LOCAL_ARM_MODE)
$(ANDROID_MAKE_NAMES): ANDROID_EXPORT_ALL_SYMBOLS := $(AND_LOCAL_EXPORT_ALL_SYMBOLS)
$(ANDROID_MAKE_NAMES): ANDROID_MAKE_TYPE := $(if $(strip $(filter prog,$(TARGET_TYPE))),BUILD_EXECUTABLE,$(if $(strip $(SRCS)),BUILD_STATIC_LIBRARY,BUILD_COPY_HEADERS))
$(ANDROID_MAKE_NAMES): ANDROID_STATIC_LIBS := $(foreach library,$(LIBS),$(if $(findstring $(strip $(BUILD_ROOT)/installed_lib/$(BUILD_ARCH)/lib$(library)$(TARGET_NAME_SUFFIX).a), $(ALL_LIBS)),lib$(library),))
$(ANDROID_MAKE_NAMES): ANDROID_SHARED_LIBS := $(foreach library,$(LIBS),$(if $(findstring $(strip $(BUILD_ROOT)/installed_lib/$(BUILD_ARCH)/lib$(library)$(TARGET_NAME_SUFFIX).so), $(SHARED_LIB_FULLNAMES)),lib$(library),))



$(ANDROID_MAKE_NAMES): FORCE
	$(quiet) echo "LOCAL_PATH := \$$(call my-dir)" > $@
	$(quiet) echo "include \$$(CLEAR_VARS)" >> $@
	$(quiet) echo "" >> $@
	$(quiet) echo "LOCAL_SRC_FILES := \\" >> $@
	$(quiet) $(call output_list,$(ANDROID_CPP_SRCS),$@)
	$(quiet) echo "" >> $@
	$(quiet) $(call output_assembly_srcs,$(ANDROID_ASM_SRCS),$@)
	$(quiet) echo "" >> $@
	$(quiet) echo "$(ANDROID_TARGET)" >> $@
	$(quiet) echo "" >> $@
	$(quiet) $(call include_local_c_flags,$(ANDROID_EXPORT_ALL_SYMBOLS),$@)
	$(quiet) $(call undo_warnings_as_errors,$@)
	$(quiet) echo "" >> $@
	$(quiet) echo "$(ANDROID_ARM_MODE)" >> $@
	$(quiet) echo "" >> $@
	$(quiet) echo "LOCAL_STATIC_LIBRARIES := $(ANDROID_STATIC_LIBS)" >> $@
	$(quiet) echo "" >> $@
	$(quiet) echo "LOCAL_SHARED_LIBRARIES := $(ANDROID_SHARED_LIBS)" >> $@
	$(quiet) echo "" >> $@
	$(quiet) echo "LOCAL_C_INCLUDES := \\" >> $@
	$(quiet) $(call output_list,$(ANDROID_C_INC),$@)
	$(quiet) echo "" >> $@
	$(quiet) echo "LOCAL_COPY_HEADERS_TO := \$$(PV_COPY_HEADERS_TO)" >> $@
	$(quiet) echo "" >> $@
	$(quiet) echo "LOCAL_COPY_HEADERS := \\" >> $@
	$(quiet) $(call output_list,$(ANDROID_HDRS),$@)
	$(quiet) $(call include_system_extras,$(ANDROID_MAKE_TYPE),$@)
	$(quiet) echo "" >> $@
	$(quiet) echo "include \$$($(ANDROID_MAKE_TYPE))" >> $@


endif


android_make: $(ANDROID_MAKE_NAMES)
CMD_COMPLETION_TARGETS += android_make android_clean

