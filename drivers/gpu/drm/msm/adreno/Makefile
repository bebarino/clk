# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/gpu/drm/msm

msm-y += adreno_device.o \
	 adreno_gpu.o \
	 a3xx_gpu.o \
	 a4xx_gpu.o \
	 a5xx_gpu.o \
	 a5xx_power.o \
	 a5xx_preempt.o \
	 a6xx_gpu.o \
	 a6xx_gmu.o \
	 a6xx_hfi.o

msm-$(CONFIG_DEBUG_FS) += a5xx_debugfs.o
