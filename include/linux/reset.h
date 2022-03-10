/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _LINUX_RESET_H_
#define _LINUX_RESET_H_

struct device_d;
struct reset_control;

#ifdef CONFIG_RESET_CONTROLLER

int reset_control_status(struct reset_control *rstc);
int reset_control_reset(struct reset_control *rstc);
int reset_control_assert(struct reset_control *rstc);
int reset_control_deassert(struct reset_control *rstc);

struct reset_control *reset_control_get(struct device_d *dev, const char *id);
struct reset_control *of_reset_control_get(struct device_node *node,
					   const char *id);
void reset_control_put(struct reset_control *rstc);

int __must_check device_reset(struct device_d *dev);

int __must_check device_reset_us(struct device_d *dev, int us);

int __must_check device_reset_all(struct device_d *dev);

#else

static inline int reset_control_status(struct reset_control *rstc)
{
	return 0;
}

static inline int reset_control_reset(struct reset_control *rstc)
{
	return 0;
}

static inline int reset_control_assert(struct reset_control *rstc)
{
	return 0;
}

static inline int reset_control_deassert(struct reset_control *rstc)
{
	return 0;
}

static inline struct reset_control *
reset_control_get(struct device_d *dev, const char *id)
{
	return NULL;
}

static inline void reset_control_put(struct reset_control *rstc)
{
}

static inline int device_reset_us(struct device_d *dev, int us)
{
	return 0;
}

static inline int device_reset(struct device_d *dev)
{
	return 0;
}

static inline int device_reset_all(struct device_d *dev)
{
	return 0;
}

#endif /* CONFIG_RESET_CONTROLLER */

#endif
