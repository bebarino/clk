# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/gpu/drm/msm

msm-y += dpu_core_irq.o \
	 dpu_core_perf.o \
	 dpu_crtc.o \
	 dpu_encoder.o \
	 dpu_encoder_phys_cmd.o \
	 dpu_encoder_phys_vid.o \
	 dpu_formats.o \
	 dpu_hw_blk.o \
	 dpu_hw_catalog.o \
	 dpu_hw_cdm.o \
	 dpu_hw_ctl.o \
	 dpu_hw_interrupts.o \
	 dpu_hw_intf.o \
	 dpu_hw_lm.o \
	 dpu_hw_pingpong.o \
	 dpu_hw_sspp.o \
	 dpu_hw_top.o \
	 dpu_hw_util.o \
	 dpu_hw_vbif.o \
	 dpu_io_util.o \
	 dpu_irq.o \
	 dpu_kms.o \
	 dpu_mdss.o \
	 dpu_plane.o \
	 dpu_power_handle.o \
	 dpu_rm.o \
	 dpu_vbif.o

msm-$(CONFIG_DEBUG_FS) += dpu_dbg.o
