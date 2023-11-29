// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2024 Google LLC
 *
 * This driver provides the ability to control GPIOs on the Chrome OS EC.
 * There isn't any direction control, and setting values on GPIOs is only
 * possible when the system is unlocked.
 */

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/gpio/driver.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

/* Setting gpios is only supported when the system is unlocked */
static void cros_ec_gpio_set(struct gpio_chip *gc, unsigned int gpio, int val)
{
	const char *name = gc->names[gpio];
	struct cros_ec_device *cros_ec = gpiochip_get_data(gc);
	struct ec_params_gpio_set params = {
		.val = val,
	};
	int ret;
	ssize_t copied;

	copied = strscpy(params.name, name, sizeof(params.name));
	if (copied < 0)
		return;

	ret = cros_ec_cmd(cros_ec, 0, EC_CMD_GPIO_SET, &params,
			  sizeof(params), NULL, 0);
	if (ret < 0)
		dev_err(gc->parent, "error setting gpio%d (%s) on EC: %d\n", gpio, name, ret);
}

static int cros_ec_gpio_get(struct gpio_chip *gc, unsigned int gpio)
{
	const char *name = gc->names[gpio];
	struct cros_ec_device *cros_ec = gpiochip_get_data(gc);
	struct ec_params_gpio_get params;
	struct ec_response_gpio_get response;
	int ret;
	ssize_t copied;

	copied = strscpy(params.name, name, sizeof(params.name));
	if (copied < 0)
		return -EINVAL;

	ret = cros_ec_cmd(cros_ec, 0, EC_CMD_GPIO_GET, &params,
			  sizeof(params), &response, sizeof(response));
	if (ret < 0) {
		dev_err(gc->parent, "error getting gpio%d (%s) on EC: %d\n", gpio, name, ret);
		return ret;
	}

	return response.val;
}

#define CROS_EC_GPIO_INPUT         BIT(8)
#define CROS_EC_GPIO_OUTPUT        BIT(9)

static int cros_ec_gpio_get_direction(struct gpio_chip *gc, unsigned int gpio)
{
	const char *name = gc->names[gpio];
	struct cros_ec_device *cros_ec = gpiochip_get_data(gc);
	struct ec_params_gpio_get_v1 params = {
		.subcmd = EC_GPIO_GET_INFO,
		.get_info.index = gpio,
	};
	struct ec_response_gpio_get_v1 response;
	int ret;

	ret = cros_ec_cmd(cros_ec, 1, EC_CMD_GPIO_GET, &params,
			  sizeof(params), &response, sizeof(response));
	if (ret < 0) {
		dev_err(gc->parent, "error getting direction of gpio%d (%s) on EC: %d\n", gpio, name, ret);
		return ret;
	}

	if (response.get_info.flags & CROS_EC_GPIO_INPUT)
		return GPIO_LINE_DIRECTION_IN;

	if (response.get_info.flags & CROS_EC_GPIO_OUTPUT)
		return GPIO_LINE_DIRECTION_OUT;

	return -EINVAL;
}

static int cros_ec_gpio_request(struct gpio_chip *chip, unsigned gpio_pin)
{
	if (gpio_pin < chip->ngpio)
		return 0;

	return -EINVAL;
}

/* Query EC for all gpio line names */
static int cros_ec_gpio_init_names(struct cros_ec_device *cros_ec, struct gpio_chip *gc)
{
	struct ec_params_gpio_get_v1 params = {
		.subcmd = EC_GPIO_GET_INFO,
	};
	struct ec_response_gpio_get_v1 response;
	int ret, i;
	/* EC may not NUL terminate */
	size_t name_len = sizeof(response.get_info.name) + 1;
	ssize_t copied;
	const char **names;
	char *str;

	names = devm_kcalloc(gc->parent, gc->ngpio, sizeof(*names), GFP_KERNEL);
	if (!names)
		return -ENOMEM;
	gc->names = names;

	str = devm_kcalloc(gc->parent, gc->ngpio, name_len, GFP_KERNEL);
	if (!str)
		return -ENOMEM;

	/* Get gpio line names one at a time */
	for (i = 0; i < gc->ngpio; i++) {
		params.get_info.index = i;
		ret = cros_ec_cmd(cros_ec, 1, EC_CMD_GPIO_GET, &params,
				  sizeof(params), &response, sizeof(response));
		if (ret < 0) {
			dev_err_probe(gc->parent, ret, "error getting gpio%d info\n", i);
			return ret;
		}

		names[i] = str;
		copied = strscpy(str, response.get_info.name, name_len);
		if (copied < 0)
			return copied;

		str += copied + 1;
	}

	return 0;
}

/* Query EC for number of gpios */
static int cros_ec_gpio_ngpios(struct cros_ec_device *cros_ec)
{
	struct ec_params_gpio_get_v1 params = {
		.subcmd = EC_GPIO_GET_COUNT,
	};
	struct ec_response_gpio_get_v1 response;
	int ret;

	ret = cros_ec_cmd(cros_ec, 1, EC_CMD_GPIO_GET, &params,
			  sizeof(params), &response, sizeof(response));
	if (ret < 0)
		return ret;

	return response.get_count.val;
}

static int cros_ec_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cros_ec_device *cros_ec = dev_get_drvdata(dev->parent);
	struct gpio_chip *gc;
	int ngpios;
	int ret;

	ngpios = cros_ec_gpio_ngpios(cros_ec);
	if (ngpios < 0) {
		dev_err_probe(dev, ngpios, "error getting gpio count\n");
		return ngpios;
	}

	gc = devm_kzalloc(&pdev->dev, sizeof(*gc), GFP_KERNEL);
	if (!gc)
		return -ENOMEM;

	gc->ngpio = ngpios;
	gc->parent = dev;
	ret = cros_ec_gpio_init_names(cros_ec, gc);
	if (ret)
		return ret;

	gc->can_sleep = true;
	gc->label = dev_name(dev);
	gc->base = -1;
	gc->set = cros_ec_gpio_set;
	gc->get = cros_ec_gpio_get;
	gc->get_direction = cros_ec_gpio_get_direction;
	gc->request = cros_ec_gpio_request;

	return devm_gpiochip_add_data(&pdev->dev, gc, cros_ec);
}

#ifdef CONFIG_OF
static const struct of_device_id cros_ec_gpio_of_match[] = {
	{ .compatible = "google,cros-ec-gpio" },
	{}
};
MODULE_DEVICE_TABLE(of, cros_ec_gpio_of_match);
#endif

static struct platform_driver cros_ec_gpio_driver = {
	.probe = cros_ec_gpio_probe,
	.driver = {
		.name = "cros-ec-gpio",
		.of_match_table = of_match_ptr(cros_ec_gpio_of_match),
	},
};
module_platform_driver(cros_ec_gpio_driver);

MODULE_DESCRIPTION("ChromeOS EC GPIO Driver");
MODULE_LICENSE("GPL");
