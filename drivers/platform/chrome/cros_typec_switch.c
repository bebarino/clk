// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2022 Google LLC
 *
 * This driver provides the ability to configure Type-C muxes and retimers which are controlled by
 * the ChromeOS EC.
 */

#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_device.h>
#include <linux/usb/typec_altmode.h>
#include <linux/usb/typec_dp.h>
#include <linux/usb/typec_mux.h>
#include <linux/usb/typec_retimer.h>

#include <drm/drm_bridge.h>
#include <drm/drm_print.h>

struct cros_typec_dp_bridge {
	struct cros_typec_switch_data *sdata;
	bool hpd_enabled;
	struct drm_bridge bridge;
};

/* Handles and other relevant data required for each port's switches. */
struct cros_typec_port {
	int port_num;
	struct typec_mux_dev *mode_switch;
	struct typec_retimer *retimer;
	struct cros_typec_switch_data *sdata;
};

/* Driver-specific data. */
struct cros_typec_switch_data {
	struct device *dev;
	struct cros_ec_device *ec;
	bool typec_cmd_supported;
	struct cros_typec_port *ports[EC_USB_PD_MAX_PORTS];
	struct cros_typec_dp_bridge *typec_dp_bridge;
};

static int cros_typec_cmd_mux_set(struct cros_typec_switch_data *sdata, int port_num, u8 index,
				  u8 state)
{
	struct ec_params_typec_control req = {
		.port = port_num,
		.command = TYPEC_CONTROL_COMMAND_USB_MUX_SET,
		.mux_params = {
			.mux_index = index,
			.mux_flags = state,
		},
	};

	return cros_ec_cmd(sdata->ec, 0, EC_CMD_TYPEC_CONTROL, &req, sizeof(req), NULL, 0);
}

static int cros_typec_get_mux_state(unsigned long mode, struct typec_altmode *alt)
{
	int ret = -EOPNOTSUPP;
	u8 pin_assign;

	if (mode == TYPEC_STATE_SAFE) {
		ret = USB_PD_MUX_SAFE_MODE;
	} else if (mode == TYPEC_STATE_USB) {
		ret = USB_PD_MUX_USB_ENABLED;
	} else if (alt && alt->svid == USB_TYPEC_DP_SID) {
		ret = USB_PD_MUX_DP_ENABLED;
		pin_assign = mode - TYPEC_STATE_MODAL;
		if (pin_assign & DP_PIN_ASSIGN_D)
			ret |= USB_PD_MUX_USB_ENABLED;
	}

	return ret;
}

static int cros_typec_send_clear_event(struct cros_typec_switch_data *sdata, int port_num,
				       u32 events_mask)
{
	struct ec_params_typec_control req = {
		.port = port_num,
		.command = TYPEC_CONTROL_COMMAND_CLEAR_EVENTS,
		.clear_events_mask = events_mask,
	};

	return cros_ec_cmd(sdata->ec, 0, EC_CMD_TYPEC_CONTROL, &req, sizeof(req), NULL, 0);
}

static bool cros_typec_check_event(struct cros_typec_switch_data *sdata, int port_num, u32 mask)
{
	struct ec_response_typec_status resp;
	struct ec_params_typec_status req = {
		.port = port_num,
	};
	int ret;

	ret = cros_ec_cmd(sdata->ec, 0, EC_CMD_TYPEC_STATUS, &req, sizeof(req),
			  &resp, sizeof(resp));
	if (ret < 0) {
		dev_warn(sdata->dev, "EC_CMD_TYPEC_STATUS failed for port: %d\n", port_num);
		return false;
	}

	if (resp.events & mask)
		return true;

	return false;
}

/*
 * The ChromeOS EC treats both mode-switches and retimers as "muxes" for the purposes of the
 * host command API. This common function configures and verifies the retimer/mode-switch
 * according to the provided setting.
 */
static int cros_typec_configure_mux(struct cros_typec_switch_data *sdata, int port_num, int index,
				    unsigned long mode, struct typec_altmode *alt)
{
	u32 event_mask;
	u8 mux_state;
	int ret;

	ret = cros_typec_get_mux_state(mode, alt);
	if (ret < 0)
		return ret;
	mux_state = (u8)ret;

	/* Clear any old mux set done event. */
	if (index == 0)
		event_mask = PD_STATUS_EVENT_MUX_0_SET_DONE;
	else
		event_mask = PD_STATUS_EVENT_MUX_1_SET_DONE;

	ret = cros_typec_send_clear_event(sdata, port_num, event_mask);
	if (ret < 0)
		return ret;

	/* Send the set command. */
	ret = cros_typec_cmd_mux_set(sdata, port_num, index, mux_state);
	if (ret < 0)
		return ret;

	/* Check for the mux set done event. */
	if (read_poll_timeout(cros_typec_check_event, ret, ret == 0, 1000,
			      1000 * 1000UL, false, sdata, port_num, event_mask)) {
		dev_err(sdata->dev, "Timed out waiting for mux set done on index: %d, state: %d\n",
			index, mux_state);
		return -ETIMEDOUT;
	}

	return 0;
}

static int cros_typec_dp_port_switch_set(struct typec_mux_dev *mode_switch,
					 struct typec_mux_state *state)
{
	struct cros_typec_port *port;
	const struct typec_displayport_data *dp_data;
	struct cros_typec_dp_bridge *typec_dp_bridge;
	struct drm_bridge *bridge;
	bool hpd_asserted;

	port = typec_mux_get_drvdata(mode_switch);
	typec_dp_bridge = port->sdata->typec_dp_bridge;
	if (!typec_dp_bridge)
		return 0;

	bridge = &typec_dp_bridge->bridge;

	if (state->mode == TYPEC_STATE_SAFE || state->mode == TYPEC_STATE_USB) {
		if (typec_dp_bridge->hpd_enabled)
			drm_bridge_hpd_notify(bridge, connector_status_disconnected);

		return 0;
	}

	if (state->alt && state->alt->svid == USB_TYPEC_DP_SID) {
		if (typec_dp_bridge->hpd_enabled) {
			dp_data = state->data;
			hpd_asserted = dp_data->status & DP_STATUS_HPD_STATE;

			if (hpd_asserted)
				drm_bridge_hpd_notify(bridge, connector_status_connected);
			else
				drm_bridge_hpd_notify(bridge, connector_status_disconnected);
		}
	}

	return 0;
}

static int cros_typec_mode_switch_set(struct typec_mux_dev *mode_switch,
				      struct typec_mux_state *state)
{
	struct cros_typec_port *port = typec_mux_get_drvdata(mode_switch);
	struct cros_typec_switch_data *sdata = port->sdata;
	int ret;

	ret = cros_typec_dp_port_switch_set(mode_switch, state);
	if (ret)
		return ret;

	/* Mode switches have index 0. */
	if (sdata->typec_cmd_supported)
		return cros_typec_configure_mux(port->sdata, port->port_num, 0, state->mode, state->alt);

	return 0;
}

static int cros_typec_retimer_set(struct typec_retimer *retimer, struct typec_retimer_state *state)
{
	struct cros_typec_port *port = typec_retimer_get_drvdata(retimer);

	/* Retimers have index 1. */
	return cros_typec_configure_mux(port->sdata, port->port_num, 1, state->mode, state->alt);
}

static void cros_typec_unregister_switches(struct cros_typec_switch_data *sdata)
{
	int i;

	for (i = 0; i < EC_USB_PD_MAX_PORTS; i++) {
		if (!sdata->ports[i])
			continue;
		typec_retimer_unregister(sdata->ports[i]->retimer);
		typec_mux_unregister(sdata->ports[i]->mode_switch);
	}
}

static int cros_typec_register_mode_switch(struct cros_typec_port *port,
					   struct fwnode_handle *fwnode)
{
	struct typec_mux_desc mode_switch_desc = {
		.fwnode = fwnode,
		.drvdata = port,
		.name = fwnode_get_name(fwnode),
		.set = cros_typec_mode_switch_set,
	};

	port->mode_switch = typec_mux_register(port->sdata->dev, &mode_switch_desc);

	return PTR_ERR_OR_ZERO(port->mode_switch);
}

static int cros_typec_register_retimer(struct cros_typec_port *port, struct fwnode_handle *fwnode)
{
	struct typec_retimer_desc retimer_desc = {
		.fwnode = fwnode,
		.drvdata = port,
		.name = fwnode_get_name(fwnode),
		.set = cros_typec_retimer_set,
	};

	port->retimer = typec_retimer_register(port->sdata->dev, &retimer_desc);

	return PTR_ERR_OR_ZERO(port->retimer);
}

static int
cros_typec_dp_bridge_attach(struct drm_bridge *bridge,
			    enum drm_bridge_attach_flags flags)
{
	if (!(flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)) {
		DRM_ERROR("Fix bridge driver to make connector optional!\n");
		return -EINVAL;
	}

	return 0;
}

static struct cros_typec_dp_bridge *
bridge_to_cros_typec_dp_bridge(struct drm_bridge *bridge)
{
	return container_of(bridge, struct cros_typec_dp_bridge, bridge);
}

static void cros_typec_dp_bridge_hpd_enable(struct drm_bridge *bridge)
{
	struct cros_typec_dp_bridge *typec_dp_bridge;

	typec_dp_bridge = bridge_to_cros_typec_dp_bridge(bridge);
	typec_dp_bridge->hpd_enabled = true;
}

static void cros_typec_dp_bridge_hpd_disable(struct drm_bridge *bridge)
{
	struct cros_typec_dp_bridge *typec_dp_bridge;

	typec_dp_bridge = bridge_to_cros_typec_dp_bridge(bridge);
	typec_dp_bridge->hpd_enabled = false;
}

static const struct drm_bridge_funcs cros_typec_dp_bridge_funcs = {
	.attach = cros_typec_dp_bridge_attach,
	.hpd_enable = cros_typec_dp_bridge_hpd_enable,
	.hpd_disable = cros_typec_dp_bridge_hpd_disable,
};

static int cros_typec_register_dp_bridge(struct cros_typec_switch_data *sdata,
					 struct fwnode_handle *fwnode)
{
	struct cros_typec_dp_bridge *typec_dp_bridge;
	struct drm_bridge *bridge;
	struct device *dev = sdata->dev;

	typec_dp_bridge = devm_kzalloc(dev, sizeof(*typec_dp_bridge), GFP_KERNEL);
	if (!typec_dp_bridge)
		return -ENOMEM;

	typec_dp_bridge->sdata = sdata;
	sdata->typec_dp_bridge = typec_dp_bridge;
	bridge = &typec_dp_bridge->bridge;

	bridge->funcs = &cros_typec_dp_bridge_funcs;
	bridge->of_node = dev->of_node;
	bridge->type = DRM_MODE_CONNECTOR_DisplayPort;
	bridge->ops |= DRM_BRIDGE_OP_HPD;

	return devm_drm_bridge_add(dev, bridge);
}

static int cros_typec_register_port(struct cros_typec_switch_data *sdata,
				    struct fwnode_handle *fwnode)
{
	struct cros_typec_port *port;
	struct device *dev = sdata->dev;
	struct acpi_device *adev;
	struct device_node *np;
	struct fwnode_handle *port_node;
	u32 index;
	int ret;
	const char *prop_name;

	port = devm_kzalloc(dev, sizeof(*port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	adev = to_acpi_device_node(fwnode);
	if (adev)
		prop_name = "_ADR";
	np = to_of_node(fwnode);
	if (np)
		prop_name = "reg";

	if (!adev && !np)
		return dev_err_probe(fwnode->dev, -ENODEV, "Couldn't get ACPI/OF device handle\n");

	ret = fwnode_property_read_u32(fwnode, prop_name, &index);
	if (ret)
		return dev_err_probe(fwnode->dev, ret, "%s property wasn't found\n", prop_name);

	if (index >= EC_USB_PD_MAX_PORTS)
		return dev_err_probe(fwnode->dev, -EINVAL, "Invalid port index number: %u\n", index);
	port->sdata = sdata;
	port->port_num = index;
	sdata->ports[index] = port;

	port_node = fwnode;
	if (np)
		fwnode = fwnode_graph_get_port_parent(fwnode);

	if (fwnode_property_present(fwnode, "retimer-switch")) {
		ret = cros_typec_register_retimer(port, port_node);
		if (ret) {
			dev_err_probe(dev, ret, "Retimer switch register failed\n");
			goto out;
		}

		dev_dbg(dev, "Retimer switch registered for index %u\n", index);
	}

	if (fwnode_property_present(fwnode, "mode-switch")) {
		ret = cros_typec_register_mode_switch(port, port_node);
		if (ret) {
			dev_err_probe(dev, ret, "Mode switch register failed\n");
			goto out;
		}

		dev_dbg(dev, "Mode switch registered for index %u\n", index);
	}


out:
	if (np)
		fwnode_handle_put(fwnode);
	return ret;
}

static int cros_typec_register_switches(struct cros_typec_switch_data *sdata)
{
	struct device *dev = sdata->dev;
	struct fwnode_handle *devnode;
	struct fwnode_handle *fwnode;
	struct fwnode_endpoint endpoint;
	int nports, ret;

	nports = device_get_child_node_count(dev);
	if (nports == 0)
		return dev_err_probe(dev, -ENODEV, "No switch devices found\n");

	devnode = dev_fwnode(dev);
	if (fwnode_graph_get_endpoint_count(devnode, 0)) {
		fwnode_graph_for_each_endpoint(devnode, fwnode) {
			ret = fwnode_graph_parse_endpoint(fwnode, &endpoint);
			if (ret) {
				fwnode_handle_put(fwnode);
				goto err;
			}
			/* Skip if not a type-c output port */
			if (endpoint.port != 2)
				continue;

			ret = cros_typec_register_port(sdata, fwnode);
			if (ret) {
				fwnode_handle_put(fwnode);
				goto err;
			}
		}
	} else {
		device_for_each_child_node(dev, fwnode) {
			ret = cros_typec_register_port(sdata, fwnode);
			if (ret) {
				fwnode_handle_put(fwnode);
				goto err;
			}
		}
	}

	if (fwnode_property_present(devnode, "mode-switch")) {
		fwnode = fwnode_graph_get_endpoint_by_id(devnode, 0, 0, 0);
		if (fwnode) {
			ret = cros_typec_register_dp_bridge(sdata, fwnode);
			fwnode_handle_put(fwnode);
			if (ret)
				goto err;
		}
	}

	return 0;
err:
	cros_typec_unregister_switches(sdata);
	return ret;
}

static int cros_typec_switch_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cros_typec_switch_data *sdata;
	struct cros_ec_dev *ec_dev;

	sdata = devm_kzalloc(dev, sizeof(*sdata), GFP_KERNEL);
	if (!sdata)
		return -ENOMEM;

	sdata->dev = dev;
	sdata->ec = dev_get_drvdata(pdev->dev.parent);

	ec_dev = dev_get_drvdata(&sdata->ec->ec->dev);
	if (!ec_dev)
		return -EPROBE_DEFER;

	sdata->typec_cmd_supported = cros_ec_check_features(ec_dev, EC_FEATURE_TYPEC_AP_MUX_SET);

	platform_set_drvdata(pdev, sdata);

	return cros_typec_register_switches(sdata);
}

static void cros_typec_switch_remove(struct platform_device *pdev)
{
	struct cros_typec_switch_data *sdata = platform_get_drvdata(pdev);

	cros_typec_unregister_switches(sdata);
}

#ifdef CONFIG_ACPI
static const struct acpi_device_id cros_typec_switch_acpi_id[] = {
	{ "GOOG001A", 0 },
	{}
};
MODULE_DEVICE_TABLE(acpi, cros_typec_switch_acpi_id);
#endif

#ifdef CONFIG_OF
static const struct of_device_id cros_typec_switch_of_match_table[] = {
	{ .compatible = "google,cros-ec-typec-switch" },
	{}
};
MODULE_DEVICE_TABLE(of, cros_typec_switch_of_match_table);
#endif

static struct platform_driver cros_typec_switch_driver = {
	.driver	= {
		.name = "cros-typec-switch",
		.acpi_match_table = ACPI_PTR(cros_typec_switch_acpi_id),
		.of_match_table = of_match_ptr(cros_typec_switch_of_match_table),
	},
	.probe = cros_typec_switch_probe,
	.remove_new = cros_typec_switch_remove,
};

module_platform_driver(cros_typec_switch_driver);

MODULE_AUTHOR("Prashant Malani <pmalani@chromium.org>");
MODULE_DESCRIPTION("ChromeOS EC Type-C Switch control");
MODULE_LICENSE("GPL");
