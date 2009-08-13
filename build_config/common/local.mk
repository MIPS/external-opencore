# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

CML2_START_RULES := start.cml
CML2_BASE_OUTPUT_NAME := pv_config

include $(MK)/cml2.mk

# The following lines should come after the inclusion of $(MK)/cml2.mk

# define some basic preprocessor conditionals that may be needed
# to get the pv_config.h file to be included
#Replace double qoutes, escape spaces and prefix -D to each flag.
XCONFIG_FLAGS:=$(call process_config_macros,$(strip $(CONFIG_FLAGS)))

# $(info config_flags = $(CONFIG_FLAGS), xconfig_flags = $(XCONFIG_FLAGS))

CPPFLAGS += $(XCONFIG_FLAGS)

INCDIRS += -I$(SRC_ROOT)/oscl/oscl/config/$(BUILD_ARCH) -I$(SRC_ROOT)/oscl/oscl/config/shared

INCDEPLIST := $(SRC_ROOT)/oscl/oscl/config/$(BUILD_ARCH),$(SRC_ROOT)/oscl/oscl/config/shared

