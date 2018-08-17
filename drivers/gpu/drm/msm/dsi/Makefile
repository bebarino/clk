# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/gpu/drm/msm

msm-y += dsi.o \
	 dsi_cfg.o \
	 dsi_host.o \
	 dsi_manager.o

msm-y += phy/
msm-$(CONFIG_DRM_MSM_DSI_PLL) += pll/
