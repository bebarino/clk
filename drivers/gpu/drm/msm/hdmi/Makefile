# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/gpu/drm/msm

msm-y += hdmi.o \
	 hdmi_audio.o \
	 hdmi_bridge.o \
	 hdmi_connector.o \
	 hdmi_i2c.o \
	 hdmi_phy.o \
	 hdmi_phy_8960.o \
	 hdmi_phy_8x60.o \
	 hdmi_phy_8x74.o

msm-$(CONFIG_COMMON_CLK) += hdmi_pll_8960.o
msm-$(CONFIG_COMMON_CLK) += hdmi_phy_8996.o

msm-$(CONFIG_DRM_MSM_HDMI_HDCP) += hdmi_hdcp.o
