# Get the current local path as the first operation
SHARED_LIB_LOCAL_PATH := $(call get_makefile_dir)




define process_aggregate_lib_target
$(eval CUMULATIVE_TARGET_LIST:= $(call clearvar))
$(eval CUMULATIVE_MAKEFILES := $(call clearvar))
$(eval COMPONENT_MAKEFILES:= $(patsubst %,$(SRC_ROOT)%,$(patsubst %,%/local.mk,$(call remove_quotes,$(AGGREGATE_LIBDIRS_$1)))))
$(eval AGGREGATE_LIB:=$1)
$(eval include $(COMPONENT_MAKEFILES))
$(eval $1_CUMULATIVE_TARGET_LIST := $(CUMULATIVE_TARGET_LIST))
$(eval $1_CUMULATIVE_MAKEFILES := $(strip $(CUMULATIVE_MAKEFILES)))
endef

ifndef shared_lib_template
define shared_lib_template
# Pseudo-targets for libraries. With this, we can use "make lib$(TARGET)" instead of "make $(DESTDIR)/lib%$(TARGET_NAME_SUFFIX).$(LIB_EXT)"
# # E.g., make libpvplayer
shared: $(DESTDIR)/lib$(1)$(TARGET_NAME_SUFFIX).$(SHARED_LIB_EXT)
lib$(1): $(DESTDIR)/lib$(1)$(TARGET_NAME_SUFFIX).$(SHARED_LIB_EXT)
$(DESTDIR)/lib$(1)$(TARGET_NAME_SUFFIX).$(SHARED_LIB_EXT): $(foreach lib,$($(1)_CUMULATIVE_TARGET_LIST),$($(lib)_$(1)_fullname)) $(foreach shlib,$(strip $(call remove_quotes,$(MODS_$(1)))),$(patsubst -l%,$(DESTDIR)/lib%$(TARGET_NAME_SUFFIX).$(SHARED_LIB_EXT),$(shlib)))
	$(eval $(1)_fullname:=$(DESTDIR)/lib$1$(TARGET_NAME_SUFFIX).$(SHARED_LIB_EXT))
	@echo "[make] Building $$@..."
	$$(call create_objdir,$$(@D))
	$(eval $(1)_SUPPORTED_LIBS_FULLNAME:=$(foreach shlib,$(call remove_quotes, $(SUPPORTED_LIBS_$1)),$($(shlib)_$(1)_fullname)))
	$$(call generate_shared_lib,$$@,$$^,$(strip $($(1)$(TARGET_NAME_SUFFIX)_PRELINK)),$($1_LDFLAGS), $($(1)_SUPPORTED_LIBS_FULLNAME))
	@echo "[make] DONE building $$@."
endef
endif


# This template defines a double-colon rule to build up the aggregate library
# from its component libraries.
define aggregate_staticlib_component_template
lib$(1): $(DESTDIR)/lib$(1)$(TARGET_NAME_SUFFIX).$(STAT_LIB_EXT)
$(DESTDIR)/lib$(1)$(TARGET_NAME_SUFFIX).$(STAT_LIB_EXT):: $(foreach lib,$($(1)_CUMULATIVE_TARGET_LIST),$($(lib)_$(1)_compiled_objs)) $(foreach modlib,$(strip $(call remove_quotes,$(MODS_$(1)))),$(foreach lib,$($(patsubst -l%,%,$(modlib))_CUMULATIVE_TARGET_LIST),$($(lib)_$(patsubst -l%,%,$(modlib))_compiled_objs)))
	$(eval $(1)_fullname:=$(DESTDIR)/lib$1$(TARGET_NAME_SUFFIX).$(STAT_LIB_EXT))
	@echo Building STATIC AGGREGATE LIB $$@
	$$(call create_objdir,$$(@D))
	$$(call generate_static_lib,$$@,$$^)
	@echo DONE
endef


# process the elements of TARGET_aggregate
AGGREGATE_LIB_TARGET_LIST := $(call remove_quotes,$(TARGET_aggregate))

# process the elements of TESTAPPS
APP_TARGET_LIST := $(call remove_quotes,$(TESTAPPS))
#process the elements of LIBS-static
STATIC_LIBS_TARGET_LIST := $(patsubst -l%,%,$(call remove_quotes,$(LIBS_static)))

define aggregate_static_template
	$(eval $(call aggregate_staticlib_component_template,$1))
endef

define aggregate_shared_template
	$(eval $(call shared_lib_template,$1))
endef

#################
# set the default library mode to shared-archive
DEFAULT_LIBTYPE := shared-archive
$(strip $(foreach target,$(AGGREGATE_LIB_TARGET_LIST),$(call process_aggregate_lib_target,$(target))))
$(foreach lib,$(AGGREGATE_LIB_TARGET_LIST),$(eval $(call aggregate_$(call remove_quotes,$($(lib)_aggregate_libtype))_template,$(lib))))
SHARED_LIB_FULLNAMES := $(patsubst %,$(DESTDIR)/%, $(patsubst %,lib%$(TARGET_NAME_SUFFIX).$(SHARED_LIB_EXT),$(AGGREGATE_LIB_TARGET_LIST)))
#################
define lib_include_template
	$(eval $1_MAKEFILE := $(patsubst %,$(SRC_ROOT)%,$(patsubst %,%/local.mk,$1)))
	$(eval include $($1_MAKEFILE))
endef

# set the default library mode to static [Is this needed?]
DEFAULT_LIBTYPE := static
$(foreach mk,$(strip $(call remove_quotes,$(LIBDIR_static))),$(eval $(call lib_include_template,$(mk))))
ALL_LIBS := $(patsubst %,$(DESTDIR)/%, $(patsubst %,lib%$(TARGET_NAME_SUFFIX).$(STAT_LIB_EXT),$(STATIC_LIBS_TARGET_LIST)))
#################
define mk_include_template
	$(eval $1_MAKEFILE := $(patsubst %,$(SRC_ROOT)%,$(patsubst %,%/local.mk,$(call remove_quotes,$(TESTAPP_DIR_$1)))))
	$(eval include $($1_MAKEFILE))
endef

# include test app makefiles 
$(foreach mk,$(strip $(call remove_quotes,$(APP_TARGET_LIST))),$(eval $(call mk_include_template,$(mk))))
ALL_BINS := $(patsubst %,$(BUILD_ROOT)/bin/$(OUTPUT_DIR_COMPONENT)/%$(TARGET_EXT),$(APP_TARGET_LIST))
#################


CMD_COMPLETION_TARGETS += $(patsubst %,lib%,$(AGGREGATE_LIB_TARGET_LIST) $(STATIC_LIBS_TARGET_LIST)) $(APP_TARGET_LIST)

default: all 

all: shared static $(ALL_BINS)

shared: 
static: $(ALL_LIBS)

LOCAL_PATH := $(SHARED_LIB_LOCAL_PATH)
AGGREGATE_LIBS_MAKEFILE := 1
-include $(PLATFORM_EXTRAS)

