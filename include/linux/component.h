/* SPDX-License-Identifier: GPL-2.0 */
#ifndef COMPONENT_H
#define COMPONENT_H

#include <linux/stddef.h>
#include <linux/device.h>

struct component_match;

/**
 * struct component_ops - callbacks for component drivers
 *
 * Components are registered with component_add() and unregistered with
 * component_del().
 */
struct component_ops {
	/**
	 * @bind:
	 *
	 * Called through component_bind_all() when the aggregate driver is
	 * ready to bind the overall driver.
	 */
	int (*bind)(struct device *comp, struct device *master,
		    void *master_data);
	/**
	 * @unbind:
	 *
	 * Called through component_unbind_all() when the aggregate driver is
	 * ready to bind the overall driver, or when component_bind_all() fails
	 * part-ways through and needs to unbind some already bound components.
	 */
	void (*unbind)(struct device *comp, struct device *master,
		       void *master_data);
};

int component_add(struct device *, const struct component_ops *);
int component_add_typed(struct device *dev, const struct component_ops *ops,
	int subcomponent);
void component_del(struct device *, const struct component_ops *);

int component_bind_all(struct device *master, void *master_data);
void component_unbind_all(struct device *master, void *master_data);

/**
 * struct component_master_ops - callback for the aggregate driver
 *
 * Aggregate drivers are registered with component_master_add_with_match() and
 * unregistered with component_master_del().
 */
struct component_master_ops {
	/**
	 * @bind:
	 *
	 * Called when all components or the aggregate driver, as specified in
	 * the match list passed to component_master_add_with_match(), are
	 * ready. Usually there are 3 steps to bind an aggregate driver:
	 *
	 * 1. Allocate a structure for the aggregate driver.
	 *
	 * 2. Bind all components to the aggregate driver by calling
	 *    component_bind_all() with the aggregate driver structure as opaque
	 *    pointer data.
	 *
	 * 3. Register the aggregate driver with the subsystem to publish its
	 *    interfaces.
	 *
	 * Note that the lifetime of the aggregate driver does not align with
	 * any of the underlying &struct device instances. Therefore devm cannot
	 * be used and all resources acquired or allocated in this callback must
	 * be explicitly released in the @unbind callback.
	 */
	int (*bind)(struct device *master);
	/**
	 * @unbind:
	 *
	 * Called when either the aggregate driver, using
	 * component_master_del(), or one of its components, using
	 * component_del(), is unregistered.
	 */
	void (*unbind)(struct device *master);
};

struct aggregate_device {
	const struct component_master_ops *ops;
	struct device *parent;
	struct device dev;
	struct component_match *match;
	struct aggregate_driver *adrv;

	int id;
};

static inline struct aggregate_device *to_aggregate_device(struct device *d)
{
	return container_of(d, struct aggregate_device, dev);
}

/**
 * struct aggregate_driver - Aggregate driver (made up of other drivers)
 * @driver: device driver
 * @match: component match list
 */
struct aggregate_driver {
	/**
	 * @probe:
	 *
	 * Called when all components or the aggregate driver, as specified in
	 * the @match list are
	 * ready. Usually there are 3 steps to bind an aggregate driver:
	 *
	 * 1. Allocate a struct aggregate_driver.
	 *
	 * 2. Bind all components to the aggregate driver by calling
	 *    component_bind_all() with the aggregate driver structure as opaque
	 *    pointer data.
	 *
	 * 3. Register the aggregate driver with the subsystem to publish its
	 *    interfaces.
	 */
	int (*probe)(struct aggregate_device *adev);
	/**
	 * @remove:
	 *
	 * Called when either the aggregate driver, using
	 * component_aggregate_unregister(), or one of its components, using
	 * component_del(), is unregistered.
	 */
	void (*remove)(struct aggregate_device *adev);
	/**
	 * @shutdown:
	 *
	 * Called when the system is shutting down.
	 */
	void (*shutdown)(struct aggregate_device *adev);

	struct device_driver	driver;
};

static inline struct aggregate_driver *to_aggregate_driver(struct device_driver *d)
{
	if (!d)
		return NULL;

	return container_of(d, struct aggregate_driver, driver);
}

int component_aggregate_register(struct device *parent,
	struct aggregate_driver *adrv, struct component_match *match);
void component_aggregate_unregister(struct device *parent,
	struct aggregate_driver *adrv);

void component_master_del(struct device *,
	const struct component_master_ops *);

int component_master_add_with_match(struct device *,
	const struct component_master_ops *, struct component_match *);
void component_match_add_release(struct device *master,
	struct component_match **matchptr,
	void (*release)(struct device *, void *),
	int (*compare)(struct device *, void *), void *compare_data);
void component_match_add_typed(struct device *master,
	struct component_match **matchptr,
	int (*compare_typed)(struct device *, int, void *), void *compare_data);

/**
 * component_match_add - add a component match entry
 * @master: device with the aggregate driver
 * @matchptr: pointer to the list of component matches
 * @compare: compare function to match against all components
 * @compare_data: opaque pointer passed to the @compare function
 *
 * Adds a new component match to the list stored in @matchptr, which the @master
 * aggregate driver needs to function. The list of component matches pointed to
 * by @matchptr must be initialized to NULL before adding the first match. This
 * only matches against components added with component_add().
 *
 * The allocated match list in @matchptr is automatically released using devm
 * actions.
 *
 * See also component_match_add_release() and component_match_add_typed().
 */
static inline void component_match_add(struct device *master,
	struct component_match **matchptr,
	int (*compare)(struct device *, void *), void *compare_data)
{
	component_match_add_release(master, matchptr, NULL, compare,
				    compare_data);
}

#endif
