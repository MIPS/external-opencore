# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

CML2_START_RULES := start.cml
CML2_BASE_OUTPUT_NAME := pv_config

include $(MK)/cml2.mk

# The following lines should come after the inclusion of $(MK)/cml2.mk
INCDIRS += -I$(SRC_ROOT)/oscl/oscl/config/$(BUILD_ARCH) -I$(SRC_ROOT)/oscl/oscl/config/shared

INCDEPLIST := $(SRC_ROOT)/oscl/oscl/config/$(BUILD_ARCH),$(SRC_ROOT)/oscl/oscl/config/shared

