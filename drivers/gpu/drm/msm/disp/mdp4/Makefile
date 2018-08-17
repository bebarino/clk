# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/gpu/drm/msm

msm-y += mdp4_crtc.o \
	 mdp4_dtv_encoder.o \
	 mdp4_lcdc_encoder.o \
	 mdp4_lvds_connector.o \
	 mdp4_irq.o \
	 mdp4_kms.o \
	 mdp4_plane.o

msm-$(CONFIG_DRM_MSM_DSI) += mdp4_dsi_encoder.o
msm-$(CONFIG_COMMON_CLK) += mdp4_lvds_pll.o
