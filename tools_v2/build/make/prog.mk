# -*- makefile -*-
#
# This makefile template should be included by makefiles in
# program directories.
#

# Set the directory for the local sources
LOCAL_SRCDIR :=  $(abspath $(LOCAL_PATH)/$(SRCDIR))
LOCAL_INCSRCDIR :=  $(abspath $(LOCAL_PATH)/$(INCSRCDIR))

ifeq ($(strip $(DEFAULT_LIBMODE)),release)
  XCXXFLAGS+=$(OPT_CXXFLAG)
  XCFLAGS+=$(OPT_CFLAG)
  XCXXFLAGS+=$(RELEASE_CXXFLAGS)
  XCFLAGS+=$(RELEASE_CFLAGS)
  XCPPFLAGS+=$(RELEASE_CPPFLAGS)
  OBJSUBDIR:=rel
else
  XCPPFLAGS+=$(DEBUG_CPPFLAGS)
  XCXXFLAGS+=$(DEBUG_CXXFLAGS)
  XCFLAGS+=$(DEBUG_CFLAGS)
  OBJSUBDIR:=dbg
endif

ifneq ($(strip $(OPTIMIZE_FOR_PERFORMANCE_OVER_SIZE)),true)
  XCXXFLAGS += $(OPTIMIZE_FOR_SIZE)
  XCFLAGS += $(OPTIMIZE_FOR_SIZE)
else
  XCXXFLAGS += $(OPTIMIZE_FOR_PERFORMANCE)
  XCFLAGS += $(OPTIMIZE_FOR_PERFORMANCE)
endif

OBJDIR := $(patsubst $(SRC_ROOT)/%,$(BUILD_ROOT)/%,$(abspath $(LOCAL_PATH)/$(OUTPUT_DIR_COMPONENT)/$(OBJSUBDIR)))

#
# Include a local makefile fragment for src and flags specific for an architecture.
# Include the template after expanding value for OBJDIR
#
-include $(call process_include_list,$(LOCAL_PATH),$(BUILD_ARCH).mk)


$(eval $(call set-src-and-obj-names,$(SRCS),$(LOCAL_SRCDIR)))

ifneq ($(strip $(FORCED_OBJS)),)
 # The point of this dependency is to force object rebuilds when the 
 # corresponding dependency files are missing (even if the object file exists).
 $(FORCED_OBJS): FORCE
endif


ifneq "$(MAKECMDGOALS)" "clean"
  ifneq ($(strip $(FOUND_DEPS)),)
# $(warning Including $(FOUND_DEPS))
  -include $(FOUND_DEPS)
  endif
endif


LOCAL_XINCDIRS := $(abspath $(patsubst ../%,$(LOCAL_PATH)/../%,$(patsubst -I%,%,$(XINCDIRS))))

LOCAL_TOTAL_INCDIRS := $(LOCAL_SRCDIR) $(LOCAL_INCSRCDIR) $(LOCAL_XINCDIRS)
LOCAL_ASM_INCDIRS := $(abspath $(patsubst ../%,$(LOCAL_PATH)/../%,$(XASMINCDIRS)))
LOCAL_ASM_INCDIRS := $(if $(strip $(LOCAL_ASM_INCDIRS)), $(patsubst %, $(ASM_INCLUDE_FLAG)%,$(LOCAL_ASM_INCDIRS)),)


$(COMPILED_OBJS): XPFLAGS := $(XCPPFLAGS) $(patsubst %,-I%,$(LOCAL_TOTAL_INCDIRS)) $(LOCAL_ASM_INCDIRS)
$(COMPILED_OBJS): XXFLAGS := $(XCXXFLAGS) $(call cond_flag_warnings_as_errors,$(LOCAL_DISABLE_COMPILE_WARNINGS_AS_ERRORS))
$(COMPILED_OBJS): XFLAGS := $(XCFLAGS) $(call cond_flag_warnings_as_errors,$(LOCAL_DISABLE_COMPILE_WARNINGS_AS_ERRORS))

# remove any leading / trailing whitespace
TARGET := $(strip $(TARGET))

# save compiled objects in a macro
$(TARGET)_compiled_objs := $(COMPILED_OBJS)

ifneq ($(strip $(REMOTE_DIRS)),)
# $(info remote dirs = $(REMOTE_DIRS))
$(foreach srcdir, $(strip $(REMOTE_DIRS)), $(eval $(call OBJ_TEMPLATE,$(srcdir),$(OBJDIR))))
endif


$(OBJDIR)/%.$(OBJ_EXT): $(LOCAL_SRCDIR)/%.cpp 
	$(call make-cpp-obj-and-depend,$<,$@,$(subst .$(OBJ_EXT),.d,$@),$(XPFLAGS),$(XXFLAGS))

$(OBJDIR)/%.$(OBJ_EXT): $(LOCAL_SRCDIR)/%.c
	$(call make-c-obj-and-depend,$<,$@,$(subst .$(OBJ_EXT),.d,$@),$(XPFLAGS),$(XFLAGS))


LOCAL_LIBDIRS := $(abspath $(patsubst ../%,$(LOCAL_PATH)/../%,$(patsubst $(LIBCOMPFLAG)%,%,$(XLIBDIRS))))

LOCAL_LIBDIRS := $(patsubst %,$(LIBCOMPFLAG)%,$(LOCAL_LIBDIRS)) $(LIB_DIRS)


REALTARGET := $(TARGET:%=$(BUILD_ROOT)/bin/$(OUTPUT_DIR_COMPONENT)/%$(TARGET_EXT))


TMPDEPS := $(patsubst %,$$(%_fullname),$(LIBS))

$(eval $(TARGET)_LIBDEPS := $(TMPDEPS))

$(TARGET)_LDFLAGS := $(LOCAL_LIBDIRS) $($(TARGET)_LIBDEPS) $(LDFLAGS) $(XLDFLAGS)

$(REALTARGET): $(COMPILED_OBJS) $($(TARGET)_LIBDEPS)
	@echo [make] Building $@
	$(call create_objdir,$(@D))
	$(call generate_prog,$@,$(notdir $@))
	@echo [make] DONE building $@.

ALL_BIN_INSTALLED: $(REALTARGET)

# Pseudo-targets for executables. With this, we can use "make $(TARGET)" instead of "make $(BUILD_ROOT)/bin/$(OUTPUT_DIR_COMPONENT)/%$(TARGET_EXT)"
# # E.g., make pvplayer_engine_test
$(TARGET): $(REALTARGET)

.PRECIOUS:: $(DEPS) $(COMPILED_OBJS)

TARGET_TYPE := prog

-include $(PLATFORM_EXTRAS)

TARGET_LIST := $(TARGET_LIST) $(TARGET)

run_$(TARGET)_TEST_ARGS := $(TEST_ARGS)
run_$(TARGET)_SOURCE_ARGS := $(SOURCE_ARGS)

# If the path from where the executable should be invoked is not specified, use the default LOCAL_PATH
ifeq ($(run_$(TARGET)_SOURCE_DIR), )
run_$(TARGET)_SOURCE_DIR := $(LOCAL_PATH)
endif

ifeq ($(PROGTYPE),gui_app)
  POST_LDFLAGS += $(GUI_LINKER_FLAG)
endif

ifneq ($(DISABLE_MAKE_BUILD), 1)
run_$(TARGET): $(REALTARGET)
endif

run_$(TARGET): REALTARGET := $(REALTARGET)

run_$(TARGET):
	$(call $@_pre_execution_steps,$($@_SOURCE_DIR))
	$(call cd_and_run_test,$($@_SOURCE_DIR),$(REALTARGET),$($@_TEST_ARGS),$($@_SOURCE_ARGS))

run_test: run_$(TARGET)
build_$(TARGET): $(REALTARGET)
build_test: build_$(TARGET)

SHOW_TARGETS: SHOW_$(TARGET)
SHOW_$(TARGET): MESSAGE:=run_$(TARGET) $(subst $(SRC_ROOT),,$(LOCAL_PATH))
SHOW_$(TARGET):
	@echo $(MESSAGE)
