// SPDX-License-Identifier: GPL-2.0
/*
 * Test managed platform driver
 */

#include <linux/device/driver.h>
#include <linux/platform_device.h>

#include <kunit/platform_device.h>
#include <kunit/resource.h>

/**
 * platform_device_alloc_kunit() - Allocate a KUnit test managed platform device
 * @test: test context
 * @name: device name of platform device to alloc
 * @id: identifier of platform device to alloc.
 *
 * Allocate a test managed platform device. The device is put when the test completes.
 *
 * Returns: Allocated platform device on success, NULL on failure.
 */
struct platform_device *
platform_device_alloc_kunit(struct kunit *test, const char *name, int id)
{
	struct platform_device *pdev;

	pdev = platform_device_alloc(name, id);
	if (!pdev)
		return NULL;

	if (kunit_add_action_or_reset(test, (kunit_action_t *)&platform_device_put, pdev))
		return NULL;

	return pdev;
}
EXPORT_SYMBOL_GPL(platform_device_alloc_kunit);

static void platform_device_add_kunit_exit(struct kunit_resource *res)
{
	struct platform_device *pdev = res->data;

	platform_device_unregister(pdev);
}

static bool
platform_device_alloc_kunit_match(struct kunit *test,
				  struct kunit_resource *res, void *match_data)
{
	struct platform_device *pdev = match_data;

	return res->data == pdev;
}

/**
 * platform_device_add_kunit() - Register a KUnit test managed platform device
 * @test: test context
 * @pdev: platform device to add
 *
 * Register a test managed platform device. The device is unregistered when the
 * test completes.
 *
 * Returns: 0 on success, negative errno on failure.
 */
int platform_device_add_kunit(struct kunit *test, struct platform_device *pdev)
{
	struct kunit_resource *res;
	int ret;

	ret = platform_device_add(pdev);
	if (ret)
		return ret;

	res = kunit_find_resource(test, platform_device_alloc_kunit_match, pdev);
	if (res) {
		/*
		 * Transfer the reference count of the platform device if it was
		 * allocated with platform_device_alloc_kunit(). In that case,
		 * calling platform_device_put() leads to reference count
		 * underflow because platform_device_unregister() does it for
		 * us and we call platform_device_unregister() from
		 * platform_device_add_kunit_exit().
		 *
		 * Usually callers transfer the refcount from
		 * platform_device_alloc() to platform_device_add() and simply
		 * call platform_device_unregister() when done, but with kunit
		 * we have to keep this straight by redirecting the free
		 * routine for the resource.
		 */
		res->free = platform_device_add_kunit_exit;
		kunit_put_resource(res);
	} else if (kunit_add_action_or_reset(test,
					     (kunit_action_t *)&platform_device_unregister,
					     pdev)) {
			return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(platform_device_add_kunit);

/**
 * platform_driver_register_kunit() - Register a KUnit test managed platform driver
 * @test: test context
 * @drv: platform driver to register
 *
 * Register a test managed platform driver. This allows callers to embed the
 * @drv in a container structure and use container_of() in the probe function
 * to pass information to KUnit tests. It can be assumed that the driver has
 * probed when this function returns.
 *
 * Example
 *
 * .. code-block:: c
 *
 *	struct kunit_test_context {
 *		struct platform_driver pdrv;
 *		const char *data;
 *	};
 *
 *	static inline struct kunit_test_context *
 *	to_test_context(struct platform_device *pdev)
 *	{
 *		return container_of(to_platform_driver(pdev->dev.driver),
 *				    struct kunit_test_context,
 *				    pdrv);
 *	}
 *
 *	static int kunit_platform_driver_probe(struct platform_device *pdev)
 *	{
 *		struct kunit_test_context *ctx;
 *
 *		ctx = to_test_context(pdev);
 *		ctx->data = "test data";
 *
 *		return 0;
 *	}
 *
 *	static void kunit_platform_driver_test(struct kunit *test)
 *	{
 *		struct kunit_test_context *ctx;
 *
 *		ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
 *		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
 *
 *		ctx->pdrv.probe = kunit_platform_driver_probe;
 *		ctx->pdrv.driver.name = "kunit-platform";
 *		ctx->pdrv.driver.owner = THIS_MODULE;
 *
 *		KUNIT_EXPECT_EQ(test, 0, platform_driver_register_kunit(test, &ctx->pdrv));
 *		KUNIT_EXPECT_STREQ(test, ctx->data, "test data");
 *	}
 *
 * Returns: 0 on success, negative errno on failure.
 */
int platform_driver_register_kunit(struct kunit *test,
				   struct platform_driver *drv)
{
	int ret;

	ret = platform_driver_register(drv);
	if (ret)
		return ret;

	/*
	 * Wait for the driver to probe (or at least flush out of the deferred
	 * workqueue)
	 */
	wait_for_device_probe();

	return kunit_add_action_or_reset(test,
					 (kunit_action_t *)&platform_driver_unregister,
					 drv);
}
EXPORT_SYMBOL_GPL(platform_driver_register_kunit);
