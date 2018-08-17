# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(srctree)/drivers/gpu/drm/msm
ccflags-y += -I$(srctree)/drivers/gpu/drm/msm/disp/dpu1
ccflags-$(CONFIG_DRM_MSM_DSI) += -I$(srctree)/drivers/gpu/drm/msm/dsi

obj-$(CONFIG_DRM_MSM)	+= msm.o

obj-y += adreno/
obj-y += disp/
obj-$(CONFIG_DRM_MSM_DSI) += dsi/
obj-y += edp/
obj-y += hdmi/

msm-y += msm_atomic.o \
	 msm_debugfs.o \
	 msm_drv.o \
	 msm_fb.o \
	 msm_fence.o \
	 msm_gem.o \
	 msm_gem_prime.o \
	 msm_gem_shrinker.o \
	 msm_gem_submit.o \
	 msm_gem_vma.o \
	 msm_gpu.o \
	 msm_iommu.o \
	 msm_perf.o \
	 msm_rd.o \
	 msm_ringbuffer.o \
	 msm_submitqueue.o

msm-$(CONFIG_DRM_FBDEV_EMULATION) += msm_fbdev.o
