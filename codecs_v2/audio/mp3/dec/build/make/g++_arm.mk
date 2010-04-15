# Additional assembly files for linux-arm
ifeq ($(GLOBAL_CPU_ARCH_VERSION),4)
    ASM_INCLUSION:=1
else ifeq ($(GLOBAL_CPU_ARCH_VERSION),5)
    ASM_INCLUSION:=1
else ifeq ($(GLOBAL_CPU_ARCH_VERSION),6)
    ASM_INCLUSION:=1
else
    ASM_INCLUSION:=0
endif
ifneq ($(ASM_INCLUSION),0)
SRCS+=  asm/pvmp3_polyphase_filter_window_gcc.s \
        asm/pvmp3_mdct_18_gcc.s \
        asm/pvmp3_dct_9_gcc.s \
        asm/pvmp3_dct_16_gcc.s
endif
