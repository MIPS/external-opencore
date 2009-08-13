
CML2_HDR := $(CML2_BASE_OUTPUT_NAME).h
CML2_DERIVED_MAKE := $(CML2_BASE_OUTPUT_NAME)_derived.mk
CML2_SELECTED_MAKE := $(CML2_BASE_OUTPUT_NAME)_selected.mk

# create a rule to install the CML2 config header
$(eval $(call INST_TEMPLATE,$(CML2_HDR),$(abspath $(CFG_DIR)),$(INCDESTDIR)))
$(INCDESTDIR)/ALL_HDRS_INSTALLED: $(INCDESTDIR)/$(CML2_HDR)

define include_cml2_make_segments
  include $(CFG_DIR)/$(CML2_SELECTED_MAKE)
  include $(CFG_DIR)/$(CML2_DERIVED_MAKE)
endef

define opt_include_cml2_make_segments
  -include $(CFG_DIR)/$(CML2_SELECTED_MAKE)
  -include $(CFG_DIR)/$(CML2_DERIVED_MAKE)
endef


$(eval $(call include_cml2_make_segments))

