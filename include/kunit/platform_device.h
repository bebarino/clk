/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KUNIT_PLATFORM_DRIVER_H
#define _KUNIT_PLATFORM_DRIVER_H

struct kunit;
struct platform_device;
struct platform_driver;

struct platform_device *
platform_device_alloc_kunit(struct kunit *test, const char *name, int id);
int platform_device_add_kunit(struct kunit *test, struct platform_device *pdev);

int platform_driver_register_kunit(struct kunit *test, struct platform_driver *drv);

#endif
