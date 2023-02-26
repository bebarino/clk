// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit test for platform driver infrastructure.
 */

#include <linux/platform_device.h>

#include <kunit/platform_device.h>
#include <kunit/test.h>

static const char * const kunit_devname = "kunit-platform";

/*
 * Test that platform_device_alloc_kunit() creates a platform device.
 */
static void platform_device_alloc_kunit_test(struct kunit *test)
{
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test,
			platform_device_alloc_kunit(test, kunit_devname, 1));
}

/*
 * Test that platform_device_add_kunit() registers a platform device on the
 * platform bus with the proper name and id.
 */
static void platform_device_add_kunit_test(struct kunit *test)
{
	struct platform_device *pdev;
	const char *name = kunit_devname;
	const int id = -1;

	pdev = platform_device_alloc_kunit(test, name, id);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdev);

	KUNIT_EXPECT_EQ(test, 0, platform_device_add_kunit(test, pdev));
	KUNIT_EXPECT_TRUE(test, dev_is_platform(&pdev->dev));
	KUNIT_EXPECT_STREQ(test, pdev->name, name);
	KUNIT_EXPECT_EQ(test, pdev->id, id);
}

/*
 * Test that platform_device_add_kunit() called twice with the same device name
 * and id fails the second time and properly cleans up.
 */
static void platform_device_add_kunit_twice_fails_test(struct kunit *test)
{
	struct platform_device *pdev;
	const char *name = kunit_devname;
	const int id = -1;

	pdev = platform_device_alloc_kunit(test, name, id);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdev);
	KUNIT_ASSERT_EQ(test, 0, platform_device_add_kunit(test, pdev));

	pdev = platform_device_alloc_kunit(test, name, id);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdev);

	KUNIT_EXPECT_NE(test, 0, platform_device_add_kunit(test, pdev));
}

/*
 * Test suite for struct platform_device kunit APIs
 */
static struct kunit_case platform_device_kunit_test_cases[] = {
	KUNIT_CASE(platform_device_alloc_kunit_test),
	KUNIT_CASE(platform_device_add_kunit_test),
	KUNIT_CASE(platform_device_add_kunit_twice_fails_test),
	{}
};

static struct kunit_suite platform_device_kunit_suite = {
	.name = "platform_device_kunit",
	.test_cases = platform_device_kunit_test_cases,
};

struct kunit_platform_driver_test_context {
	struct platform_driver pdrv;
	const char *data;
};

static const char * const test_data = "test data";

static inline struct kunit_platform_driver_test_context *
to_test_context(struct platform_device *pdev)
{
	return container_of(to_platform_driver(pdev->dev.driver),
			    struct kunit_platform_driver_test_context,
			    pdrv);
}

static int kunit_platform_driver_probe(struct platform_device *pdev)
{
	struct kunit_platform_driver_test_context *ctx;

	ctx = to_test_context(pdev);
	ctx->data = test_data;

	return 0;
}

/* Test that platform_driver_register_kunit() registers a driver that probes. */
static void platform_driver_register_kunit_test(struct kunit *test)
{
	struct platform_device *pdev;
	struct kunit_platform_driver_test_context *ctx;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdev = platform_device_alloc_kunit(test, kunit_devname, -1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdev);
	KUNIT_ASSERT_EQ(test, 0, platform_device_add_kunit(test, pdev));

	ctx->pdrv.probe = kunit_platform_driver_probe;
	ctx->pdrv.driver.name = kunit_devname;
	ctx->pdrv.driver.owner = THIS_MODULE;

	KUNIT_EXPECT_EQ(test, 0, platform_driver_register_kunit(test, &ctx->pdrv));
	KUNIT_EXPECT_STREQ(test, ctx->data, test_data);
}

static struct kunit_case platform_driver_kunit_test_cases[] = {
	KUNIT_CASE(platform_driver_register_kunit_test),
	{}
};

/*
 * Test suite for struct platform_driver kunit APIs
 */
static struct kunit_suite platform_driver_kunit_suite = {
	.name = "platform_driver_kunit",
	.test_cases = platform_driver_kunit_test_cases,
};

kunit_test_suites(
	&platform_device_kunit_suite,
	&platform_driver_kunit_suite,
);

MODULE_LICENSE("GPL");
