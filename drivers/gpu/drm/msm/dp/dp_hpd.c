// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "dp_hpd.h"

struct dp_hpd_private {
	struct device *dev;
	struct dp_usbpd_cb *dp_cb;
	struct dp_usbpd dp_usbpd;
};

int dp_hpd_connect(struct dp_usbpd *dp_usbpd, bool hpd)
{
	struct dp_hpd_private *hpd_priv;

	hpd_priv = container_of(dp_usbpd, struct dp_hpd_private,
					dp_usbpd);

	dp_usbpd->hpd_high = hpd;

	if (!hpd_priv->dp_cb && !hpd_priv->dp_cb->configure
				&& !hpd_priv->dp_cb->disconnect) {
		pr_err("hpd dp_cb not initialized\n");
		return -EINVAL;
	}
	if (hpd)
		hpd_priv->dp_cb->configure(hpd_priv->dev);
	else
		hpd_priv->dp_cb->disconnect(hpd_priv->dev);

	return 0;
}

struct dp_usbpd *dp_hpd_get(struct device *dev, struct dp_usbpd_cb *cb)
{
	struct dp_hpd_private *dp_hpd;

	if (!cb) {
		pr_err("invalid cb data\n");
		return ERR_PTR(-EINVAL);
	}

	dp_hpd = devm_kzalloc(dev, sizeof(*dp_hpd), GFP_KERNEL);
	if (!dp_hpd)
		return ERR_PTR(-ENOMEM);

	dp_hpd->dev = dev;
	dp_hpd->dp_cb = cb;

	dp_hpd->dp_usbpd.connect = dp_hpd_connect;

	return &dp_hpd->dp_usbpd;
}
