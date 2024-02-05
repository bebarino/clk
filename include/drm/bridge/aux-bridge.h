/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2023 Linaro Ltd.
 *
 * Author: Dmitry Baryshkov <dmitry.baryshkov@linaro.org>
 */
#ifndef DRM_AUX_BRIDGE_H
#define DRM_AUX_BRIDGE_H

#include <linux/usb/typec.h>

#include <drm/drm_connector.h>

#if IS_ENABLED(CONFIG_DRM_AUX_BRIDGE)
int drm_aux_bridge_register(struct device *parent);
#else
static inline int drm_aux_bridge_register(struct device *parent)
{
	return 0;
}
#endif

struct drm_dp_typec_bridge_desc {
	struct device *parent;
	struct device_node *of_node;
	size_t num_dp_lanes;
};

enum usb_ss_lane {
	USB_SSRX1 = 0,	/* Type-C pins B11/B10 */
	USB_SSTX1 = 1,	/* Type-C pins A2/A3 */
	USB_SSTX2 = 2,	/* Type-C pins A11/A10 */
	USB_SSRX2 = 3,	/* Type-C pins B2/B3 */
};

#define NUM_USB_SS	(USB_SSRX2 + 1)

#if IS_ENABLED(CONFIG_DRM_AUX_HPD_BRIDGE)
struct device *drm_dp_hpd_bridge_register(struct device *parent,
					  struct device_node *np);
struct device *drm_dp_typec_bridge_register(const struct drm_dp_typec_bridge_desc *desc);
void drm_aux_hpd_bridge_notify(struct device *dev, enum drm_connector_status status);
int drm_aux_typec_bridge_assign_pins(struct device *dev, u32 conf,
				     enum typec_orientation orientation,
				     enum usb_ss_lane lane_mapping[NUM_USB_SS]);
#else
static inline struct device *drm_dp_hpd_bridge_register(struct device *parent,
							struct device_node *np)
{
	return NULL;
}

static inline struct device *
drm_dp_typec_bridge_register(const struct drm_dp_typec_bridge_desc *desc)
{
	return NULL;
}

static inline void drm_aux_hpd_bridge_notify(struct device *dev, enum drm_connector_status status)
{
}
static inline int drm_aux_typec_bridge_assign_pins(struct device *dev, u32 conf,
				     enum typec_orientation orientation,
				     enum usb_ss_lane lane_mapping[NUM_USB_SS])
{
	return 0;
}
#endif

#endif
