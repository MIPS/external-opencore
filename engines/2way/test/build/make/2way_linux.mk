SYSLIBS += $(SYS_THREAD_LIB)

#$(error "2way_linux.mk has been included")
TWOWAY_TEST_DIR := ${BUILD_ROOT}/2way_test
TWOWAYFULL_TARGET := ./${TWOWAY_TARGET}


run_2way_test :: run_2way_test_common 
	$(quiet) export LD_LIBRARY_PATH=${BUILD_ROOT}/installed_lib/${BUILD_ARCH} && cd $(TWOWAY_TEST_DIR) && $(TWOWAYFULL_TARGET) $(TEST_ARGS) $(SOURCE_ARGS)


