/* SPDX-License-Identifier: GPL-2.0 */
#ifndef COMPONENT_H
#define COMPONENT_H

#include <linux/stddef.h>
#include <linux/device/driver.h>
#include <linux/refcount.h>

struct aggregate_device;
struct device;

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
	 *
	 * Deprecated: Use bind_component() instead.
	 */
	int (*bind)(struct device *comp, struct device *master,
		    void *master_data);
	/**
	 * @bind_component:
	 *
	 * Called through component_bind_all() when the aggregate driver is
	 * ready to bind the overall driver.
	 */
	int (*bind_component)(struct device *comp, struct aggregate_device *adev,
			      void *aggregate_data);
	/**
	 * @unbind:
	 *
	 * Called through component_unbind_all() when the aggregate driver is
	 * ready to bind the overall driver, or when component_bind_all() fails
	 * part-ways through and needs to unbind some already bound components.
	 *
	 * Deprecated: Use unbind_component() instead.
	 */
	void (*unbind)(struct device *comp, struct device *master,
		       void *master_data);
	/**
	 * @unbind_component:
	 *
	 * Called through component_unbind_all() when the aggregate driver is
	 * ready to unbind the overall driver, or when component_bind_all() fails
	 * part-ways through and needs to unbind some already bound components.
	 */
	int (*unbind_component)(struct device *comp, struct aggregate_device *adev,
				void *aggregate_data);
};

int component_add(struct device *, const struct component_ops *);
int component_add_typed(struct device *dev, const struct component_ops *ops,
	int subcomponent);
void component_del(struct device *, const struct component_ops *);

int component_bind_all(struct device *parent, void *data);
void component_unbind_all(struct device *parent, void *data);

struct device *aggregate_device_parent(const struct aggregate_device *adev);

/**
 * struct aggregate_driver - Aggregate driver (made up of other drivers)
 * @count: driver registration refcount
 * @driver: device driver
 */
struct aggregate_driver {
	/**
	 * @probe:
	 *
	 * Called when all components or the aggregate driver, as specified in
	 * the aggregate_device's match list are
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

	refcount_t		count;
	struct device_driver	driver;
};

static inline struct aggregate_driver *to_aggregate_driver(struct device_driver *d)
{
	if (!d)
		return NULL;

	return container_of(d, struct aggregate_driver, driver);
}

struct component_match;

int component_aggregate_register(struct device *parent,
	struct aggregate_driver *adrv, struct component_match *match);
void component_aggregate_unregister(struct device *parent,
	struct aggregate_driver *adrv);

void component_match_add_release(struct device *parent,
	struct component_match **matchptr,
	void (*release)(struct device *, void *),
	int (*compare)(struct device *, void *), void *compare_data);
void component_match_add_typed(struct device *parent,
	struct component_match **matchptr,
	int (*compare_typed)(struct device *, int, void *), void *compare_data);

/**
 * component_match_add - add a component match entry
 * @parent: device with the aggregate driver
 * @matchptr: pointer to the list of component matches
 * @compare: compare function to match against all components
 * @compare_data: opaque pointer passed to the @compare function
 *
 * Adds a new component match to the list stored in @matchptr, which the @parent
 * aggregate driver needs to function. The list of component matches pointed to
 * by @matchptr must be initialized to NULL before adding the first match. This
 * only matches against components added with component_add().
 *
 * The allocated match list in @matchptr is automatically released using devm
 * actions.
 *
 * See also component_match_add_release() and component_match_add_typed().
 */
static inline void component_match_add(struct device *parent,
	struct component_match **matchptr,
	int (*compare)(struct device *, void *), void *compare_data)
{
	component_match_add_release(parent, matchptr, NULL, compare,
				    compare_data);
}

#endif
