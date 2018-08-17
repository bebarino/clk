# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/gpu/drm/msm

msm-y += mdp5_cfg.o \
	 mdp5_ctl.o \
	 mdp5_crtc.o \
	 mdp5_encoder.o \
	 mdp5_irq.o \
	 mdp5_mdss.o \
	 mdp5_kms.o \
	 mdp5_pipe.o \
	 mdp5_mixer.o \
	 mdp5_plane.o \
	 mdp5_smp.o

msm-$(CONFIG_DRM_MSM_DSI) += mdp5_cmd_encoder.o
