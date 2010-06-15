# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvmf
SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := \
    pvmf_basic_errorinfomessage.cpp \
    pvmf_duration_infomessage.cpp \
    pvmf_event_handling.cpp \
    pvmf_format_type.cpp \
    pvmf_media_clock.cpp \
    pvmf_media_cmd.cpp \
    pvmf_media_data.cpp \
    pvmf_mempool.cpp \
    pvmf_metadata_infomessage.cpp \
    pvmf_node_interface.cpp \
    pvmf_node_interface_impl.cpp \
    pvmf_pool_buffer_allocator.cpp \
    pvmf_port_base_impl.cpp \
    pvmf_return_codes.cpp \
    pvmf_sync_util_data_queue.cpp \
    pvmf_simple_media_buffer.cpp \
    pvmf_sync_util.cpp \
    pvmf_timestamp.cpp \
    pvmi_config_and_capability_utils.cpp \
    pvmi_kvp_util.cpp \
    pvmi_oscl_file_data_stream.cpp

HDRS :=  \
    pv_interface.h \
    pv_uuid.h \
    pvmf_basic_errorinfomessage.h \
    pvmf_counted_ptr.h \
    pvmf_duration_infomessage.h \
    pvmf_durationinfomessage_extension.h \
    pvmf_errorinfomessage_extension.h \
    pvmf_event_handling.h \
    pvmf_fileformat_events.h \
    pvmf_fixedsize_buffer_alloc.h \
    pvmf_format_type.h \
    pvmf_media_clock.h \
    pvmf_media_cmd.h \
    pvmf_media_data.h \
    pvmf_media_data_impl.h \
    pvmf_media_frag_group.h \
    pvmf_media_msg.h \
    pvmf_media_msg_format_ids.h \
    pvmf_media_msg_header.h \
    pvmi_media_transfer.h \
    pvmf_mempool.h \
    pvmf_meta_data_types.h \
    pvmf_metadata_infomessage.h \
    pvmf_metadatainfomessage_extension.h \
    pvmf_node_cmd_msg.h \
    pvmf_node_interface.h \
    pvmf_node_interface_impl.h \
    pvmf_node_utils.h \
    pvmf_pool_buffer_allocator.h \
    pvmf_port_base_impl.h \
    pvmf_port_interface.h \
    pvmf_resizable_simple_mediamsg.h \
    pvmf_return_codes.h \
    pvmf_simple_media_buffer.h \
    pvmf_source_node_utils.h \
    pvmf_sync_util_data_queue.h \
    pvmf_sync_util.h \
    pvmf_timedtext.h \
    pvmf_timestamp.h \
    pvmf_video.h \
    pvmi_config_and_capability.h \
    pvmi_config_and_capability_base.h \
    pvmi_config_and_capability_observer.h \
    pvmi_config_and_capability_utils.h \
    pvmi_data_stream_interface.h \
    pvmi_datastreamuser_interface.h \
    pvmi_drm_kvp.h \
    pvmi_fileio_kvp.h  \
    pvmi_kvp.h \
    pvmi_kvp_util.h \
    pvmi_media_io_clock_extension.h \
    pvmi_media_io_observer.h \
    pvmi_mio_control.h \
    pvmi_port_config_kvp.h \
    pvmf_fileinput_settings.h \
    pvmi_ds_basic_interface.h \
    pvmi_data_stream.h

include $(MK)/library.mk
