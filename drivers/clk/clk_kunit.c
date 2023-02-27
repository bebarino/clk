// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit helpers for clk providers and consumers
 */
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <kunit/clk.h>
#include <kunit/resource.h>

/**
 * clk_prepare_enable_kunit() - Test managed clk_prepare_enable()
 * @test: The test context
 * @clk: clk to prepare and enable
 *
 * Returns: 0 on success, or negative errno on failure.
 */
int clk_prepare_enable_kunit(struct kunit *test, struct clk *clk)
{
	int ret;

	ret = clk_prepare_enable(clk);
	if (ret)
		return ret;

	return kunit_add_action_or_reset(test,
					 (kunit_action_t *)&clk_disable_unprepare,
					 clk);
}
EXPORT_SYMBOL_GPL(clk_prepare_enable_kunit);

static struct clk *__clk_get_kunit(struct kunit *test, struct clk *clk)
{
	int ret;

	if (IS_ERR(clk))
		return clk;

	ret = kunit_add_action_or_reset(test, (kunit_action_t *)&clk_put, clk);
	if (ret)
		return ERR_PTR(ret);

	return clk;
}

/**
 * clk_get_kunit() - Test managed clk_get()
 * @test: The test context
 * @dev: device for clock "consumer"
 * @con_id: clock consumer ID
 *
 * Just like clk_get(), except the clk is managed by the test case and is
 * automatically put with clk_put() after the test case concludes.
 *
 * Returns: new clk consumer or ERR_PTR on failure.
 */
struct clk *
clk_get_kunit(struct kunit *test, struct device *dev, const char *con_id)
{
	struct clk *clk;

	clk = clk_get(dev, con_id);

	return __clk_get_kunit(test, clk);
}
EXPORT_SYMBOL_GPL(clk_get_kunit);

/**
 * of_clk_get_kunit() - Test managed of_clk_get()
 * @test: The test context
 * @np: device_node for clock "consumer"
 * @index: index in 'clocks' property of @np
 *
 * Just like of_clk_get(), except the clk is managed by the test case and is
 * automatically put with clk_put() after the test case concludes.
 *
 * Returns: new clk consumer or ERR_PTR on failure.
 */
struct clk *
of_clk_get_kunit(struct kunit *test, struct device_node *np, int index)
{
	struct clk *clk;

	clk = of_clk_get(np, index);

	return __clk_get_kunit(test, clk);
}
EXPORT_SYMBOL_GPL(of_clk_get_kunit);

/**
 * clk_hw_get_clk_kunit() - Test managed clk_hw_get_clk()
 * @test: The test context
 * @hw: clk_hw associated with the clk being consumed
 * @con_id: connection ID string on device
 *
 * Just like clk_hw_get_clk(), except the clk is managed by the test case and
 * is automatically put with clk_put() after the test case concludes.
 *
 * Returns: new clk consumer or ERR_PTR on failure.
 */
struct clk *
clk_hw_get_clk_kunit(struct kunit *test, struct clk_hw *hw, const char *con_id)
{
	struct clk *clk;

	clk = clk_hw_get_clk(hw, con_id);

	return __clk_get_kunit(test, clk);
}
EXPORT_SYMBOL_GPL(clk_hw_get_clk_kunit);

/**
 * clk_hw_get_clk_prepared_enabled_kunit() - Test managed clk_hw_get_clk() + clk_prepare_enable()
 * @test: The test context
 * @hw: clk_hw associated with the clk being consumed
 * @con_id: connection ID string on device
 *
 * Just like
 *
 * .. code-block:: c
 *
 *	struct clk *clk = clk_hw_get_clk(...);
 *	clk_prepare_enable(clk);
 *
 * except the clk is managed by the test case and is automatically disabled and
 * unprepared with clk_disable_unprepare() and put with clk_put() after the
 * test case concludes.
 *
 * Returns: new clk consumer that is prepared and enabled or ERR_PTR on failure.
 */
struct clk *
clk_hw_get_clk_prepared_enabled_kunit(struct kunit *test, struct clk_hw *hw,
				      const char *con_id)
{
	int ret;
	struct clk *clk;

	clk = clk_hw_get_clk_kunit(test, hw, con_id);
	if (IS_ERR(clk))
		return clk;

	ret = clk_prepare_enable_kunit(test, clk);
	if (ret)
		return ERR_PTR(ret);

	return clk;
}
EXPORT_SYMBOL_GPL(clk_hw_get_clk_prepared_enabled_kunit);

/**
 * clk_hw_register_kunit() - Test managed clk_hw_register()
 * @test: The test context
 * @dev: device that is registering this clock
 * @hw: link to hardware-specific clock data
 *
 * Just like clk_hw_register(), except the clk registration is managed by the
 * test case and is automatically unregistered after the test case concludes.
 *
 * Returns: 0 on success or a negative errno value on failure.
 */
int clk_hw_register_kunit(struct kunit *test, struct device *dev, struct clk_hw *hw)
{
	int ret;

	ret = clk_hw_register(dev, hw);
	if (ret)
		return ret;

	return kunit_add_action_or_reset(test, (kunit_action_t *)&clk_hw_unregister, hw);
}
EXPORT_SYMBOL_GPL(clk_hw_register_kunit);

/**
 * of_clk_hw_register_kunit() - Test managed of_clk_hw_register()
 * @test: The test context
 * @node: device_node of device that is registering this clock
 * @hw: link to hardware-specific clock data
 *
 * Just like of_clk_hw_register(), except the clk registration is managed by
 * the test case and is automatically unregistered after the test case
 * concludes.
 *
 * Returns: 0 on success or a negative errno value on failure.
 */
int of_clk_hw_register_kunit(struct kunit *test, struct device_node *node, struct clk_hw *hw)
{
	int ret;

	ret = of_clk_hw_register(node, hw);
	if (ret)
		return ret;

	return kunit_add_action_or_reset(test, (kunit_action_t *)&clk_hw_unregister, hw);
}
EXPORT_SYMBOL_GPL(of_clk_hw_register_kunit);
