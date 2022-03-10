/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * (C) 2007 Pengutronix, Sascha Hauer <s.hauer@pengutronix.de>
 */

#ifndef DRIVER_H
#define DRIVER_H

#include <linux/list.h>
#include <linux/ioport.h>
#include <of.h>
#include <filetype.h>

#define FORMAT_DRIVER_NAME_ID	"%s%d"

#include <param.h>

struct filep;
struct bus_type;

struct platform_device_id {
	const char *name;
	unsigned long driver_data;
};

/** @brief Describes a particular device present in the system */
struct device_d {
	/*! This member (and 'type' described below) is used to match
	 * with a driver. This is a descriptive name and could be
	 * MPC5XXX_ether or imx_serial. Unless absolutely necessary,
	 * should not be modified directly and dev_set_name() should
	 * be used instead.
	 */
	char *name;

	/*! This member is used to store device's unique name as
	 *  obtained by calling dev_id(). Internal field, do not
	 *  access it directly.
	  */
	char *unique_name;
	/*! The id is used to uniquely identify a device in the system. The id
	 * will show up under /dev/ as the device's name. Usually this is
	 * something like eth0 or nor0. */
	int id;

	struct resource *resource;
	int num_resources;

	void *platform_data; /*! board specific information about this device */

	/*! Devices of a particular class normaly need to store more
	 * information than struct device holds.
	 */
	void *priv;
	void *type_data;     /*! In case this device is a specific device, this pointer
			      * points to the type specific device, i.e. eth_device
			      */
	struct driver_d *driver; /*! The driver for this device */

	struct list_head list;     /* The list of all devices */
	struct list_head bus_list; /* our bus            */
	struct list_head children; /* our children            */
	struct list_head sibling;
	struct list_head active;   /* The list of all devices which have a driver */

	struct device_d *parent;   /* our parent, NULL if not present */

	struct bus_type *bus;

	/*! The parameters for this device. This is used to carry information
	 * of board specific data from the board code to the device driver. */
	struct list_head parameters;

	struct list_head cdevs;

	const struct platform_device_id *id_entry;
	struct device_node *device_node;

	const struct of_device_id *of_id_entry;

	u64 dma_mask;

	unsigned long dma_offset;

	void    (*info) (struct device_d *);
	/*
	 * For devices which take longer to probe this is called
	 * when the driver should actually detect client devices
	 */
	int     (*detect) (struct device_d *);
};

/** @brief Describes a driver present in the system */
struct driver_d {
	/*! The name of this driver. Used to match to
	 * the corresponding device. */
	const char *name;

	struct list_head list;
	struct list_head bus_list; /* our bus            */

	/*! Called if an instance of a device is found */
	int     (*probe) (struct device_d *);

	/*! Called if an instance of a device is gone. */
	void     (*remove)(struct device_d *);

	struct bus_type *bus;

	const struct platform_device_id *id_table;
	const struct of_device_id *of_compatible;
};

/*@}*/	/* do not delete, doxygen relevant */

#define RW_SIZE(x)      (x)
#define RW_SIZE_MASK    0x7

/* dynamically assign the next free id */
#define DEVICE_ID_DYNAMIC	-2
/* do not use an id (only one device available */
#define DEVICE_ID_SINGLE	-1

/* Register devices and drivers.
 */
int register_driver(struct driver_d *);
int register_device(struct device_d *);

/* manualy probe a device
 * the driver need to be specified
 */
int device_probe(struct device_d *dev);

/* detect devices attached to this device (cards, disks,...) */
int device_detect(struct device_d *dev);
int device_detect_by_name(const char *devname);
void device_detect_all(void);

/* Unregister a device. This function can fail, e.g. when the device
 * has children.
 */
int unregister_device(struct device_d *);

void free_device_res(struct device_d *dev);
void free_device(struct device_d *dev);


/* Iterate over a devices children
 */
#define device_for_each_child(dev, child) \
	list_for_each_entry(child, &(dev)->children, sibling)

/* Iterate over a devices children - Safe against removal version
 */
#define device_for_each_child_safe(dev, tmpdev, child) \
	list_for_each_entry_safe(child, tmpdev, &(dev)->children, sibling)

/* Iterate through the devices of a given type. if last is NULL, the
 * first device of this type is returned. Put this pointer in as
 * 'last' to get the next device. This functions returns NULL if no
 * more devices are found.
 */
struct device_d *get_device_by_type(ulong type, struct device_d *last);
struct device_d *get_device_by_id(const char *id);
struct device_d *get_device_by_name(const char *name);

/* Find a free device id from the given template. This is archieved by
 * appending a number to the template. Dynamically created devices should
 * use this function rather than filling the id field themselves.
 */
int get_free_deviceid(const char *name_template);

char *deviceid_from_spec_str(const char *str, char **endp);

static inline const char *dev_id(const struct device_d *dev)
{
	return (dev->id != DEVICE_ID_SINGLE) ? dev->unique_name : dev->name;
}

static inline const char *dev_name(const struct device_d *dev)
{
	return dev_id(dev);
}

int dev_set_name(struct device_d *dev, const char *fmt, ...);

/*
 * get resource 'num' for a device
 */
struct resource *dev_get_resource(struct device_d *dev, unsigned long type,
				  int num);
/*
 * get resource base 'name' for a device
 */
struct resource *dev_get_resource_by_name(struct device_d *dev,
					  unsigned long type,
					  const char *name);

/*
 * exlusively request register base 'name' for a device
 */
void __iomem *dev_request_mem_region_by_name(struct device_d *dev,
					     const char *name);

/*
 * get register base 'num' for a device
 */
void *dev_get_mem_region(struct device_d *dev, int num);

/*
 * exlusively request register base 'num' for a device
 * deprecated, use dev_request_mem_resource instead
 */
void __iomem *dev_request_mem_region(struct device_d *dev, int num);

/*
 * exlusively request resource 'num' for a device
 */
struct resource *dev_request_mem_resource(struct device_d *dev, int num);

/*
 * exlusively request resource 'name' for a device
 */
struct resource *dev_request_mem_resource_by_name(struct device_d *dev, const char *name);

/*
 * exlusively request register base 'num' for a device
 * will return NULL on error
 * only used on platform like at91 where the Ressource address collision with
 * PTR errno
 */
void __iomem *dev_request_mem_region_err_null(struct device_d *dev, int num);

struct device_d *device_alloc(const char *devname, int id);

int device_add_resources(struct device_d *dev, const struct resource *res, int num);

int device_add_resource(struct device_d *dev, const char *resname,
		resource_size_t start, resource_size_t size, unsigned int flags);

int device_add_data(struct device_d *dev, void *data, size_t size);

/*
 * register a generic device
 * with only one resource
 */
struct device_d *add_generic_device(const char* devname, int id, const char *resname,
		resource_size_t start, resource_size_t size, unsigned int flags,
		void *pdata);

/*
 * register a generic device
 * with multiple resources
 */
struct device_d *add_generic_device_res(const char* devname, int id,
		struct resource *res, int nb, void *pdata);

/*
 * register a memory device
 */
static inline struct device_d *add_mem_device(const char *name, resource_size_t start,
		resource_size_t size, unsigned int flags)
{
	return add_generic_device("mem", DEVICE_ID_DYNAMIC, name, start, size,
				  IORESOURCE_MEM | flags, NULL);
}

static inline struct device_d *add_cfi_flash_device(int id, resource_size_t start,
		resource_size_t size, unsigned int flags)
{
	return add_generic_device("cfi_flash", id, NULL, start, size,
				  IORESOURCE_MEM | flags, NULL);
}

struct NS16550_plat;
static inline struct device_d *add_ns16550_device(int id, resource_size_t start,
		resource_size_t size, int flags, struct NS16550_plat *pdata)
{
	return add_generic_device("ns16550_serial", id, NULL, start, size,
				  flags, pdata);
}

#ifdef CONFIG_DRIVER_NET_DM9K
struct device_d *add_dm9000_device(int id, resource_size_t base,
		resource_size_t data, int flags, void *pdata);
#else
static inline struct device_d *add_dm9000_device(int id, resource_size_t base,
		resource_size_t data, int flags, void *pdata)
{
	return NULL;
}
#endif

#ifdef CONFIG_USB_EHCI
struct device_d *add_usb_ehci_device(int id, resource_size_t hccr,
		resource_size_t hcor, void *pdata);
#else
static inline struct device_d *add_usb_ehci_device(int id, resource_size_t hccr,
		resource_size_t hcor, void *pdata)
{
	return NULL;
}
#endif

#ifdef CONFIG_DRIVER_NET_KS8851_MLL
struct device_d *add_ks8851_device(int id, resource_size_t addr,
		resource_size_t addr_cmd, int flags, void *pdata);
#else
static inline struct device_d *add_ks8851_device(int id, resource_size_t addr,
		resource_size_t addr_cmd, int flags, void *pdata)
{
	return NULL;
}
#endif

static inline struct device_d *add_generic_usb_ehci_device(int id,
		resource_size_t base, void *pdata)
{
	return add_usb_ehci_device(id, base + 0x100, base + 0x140, pdata);
}

static inline struct device_d *add_gpio_keys_device(int id, void *pdata)
{
	return add_generic_device_res("gpio_keys", id, 0, 0, pdata);
}

/* linear list over all available devices
 */
extern struct list_head device_list;

/* linear list over all available drivers
 */
extern struct list_head driver_list;

/* linear list over all active devices
 */
extern struct list_head active_device_list;

/* Iterate over all devices
 */
#define for_each_device(dev) list_for_each_entry(dev, &device_list, list)

/* Iterate over all drivers
 */
#define for_each_driver(drv) list_for_each_entry(drv, &driver_list, list)

/* Find a driver with the given name. Currently the filesystem implementation
 * uses this to get the driver from the name the user specifies with the
 * mount command
 */
struct driver_d *get_driver_by_name(const char *name);

struct cdev;

/* These are used by drivers which work with direct memory accesses */
ssize_t mem_read(struct cdev *cdev, void *buf, size_t count, loff_t offset, ulong flags);
ssize_t mem_write(struct cdev *cdev, const void *buf, size_t count, loff_t offset, ulong flags);
ssize_t mem_copy(struct device_d *dev, void *dst, const void *src,
		 resource_size_t count, resource_size_t offset,
		 unsigned long flags);

int generic_memmap_ro(struct cdev *dev, void **map, int flags);
int generic_memmap_rw(struct cdev *dev, void **map, int flags);

static inline int dev_open_default(struct device_d *dev, struct filep *f)
{
	return 0;
}

static inline int dev_close_default(struct device_d *dev, struct filep *f)
{
	return 0;
}

struct bus_type {
	char *name;
	int (*match)(struct device_d *dev, struct driver_d *drv);
	int (*probe)(struct device_d *dev);
	void (*remove)(struct device_d *dev);

	struct device_d *dev;

	struct list_head list;
	struct list_head device_list;
	struct list_head driver_list;
};

int bus_register(struct bus_type *bus);
int device_match(struct device_d *dev, struct driver_d *drv);

extern struct list_head bus_list;

/* Iterate over all buses
 */
#define for_each_bus(bus) list_for_each_entry(bus, &bus_list, list)

/* Iterate over all devices of a bus
 */
#define bus_for_each_device(bus, dev) list_for_each_entry(dev, &(bus)->device_list, bus_list)

/* Iterate over all drivers of a bus
 */
#define bus_for_each_driver(bus, drv) list_for_each_entry(drv, &(bus)->driver_list, bus_list)

extern struct bus_type platform_bus;

int platform_driver_register(struct driver_d *drv);

/* register_driver_macro() - Helper macro for drivers that don't do
 * anything special in module registration. This eliminates a lot of
 * boilerplate. Each module may only use this macro once.
 */
#define register_driver_macro(level,bus,drv)		\
	static int __init drv##_register(void)		\
	{						\
		return bus##_driver_register(&drv);	\
	}						\
	level##_initcall(drv##_register)

#define core_platform_driver(drv)	\
	register_driver_macro(core,platform,drv)
#define postcore_platform_driver(drv)	\
	register_driver_macro(postcore,platform,drv)
#define coredevice_platform_driver(drv)	\
	register_driver_macro(coredevice,platform,drv)
#define device_platform_driver(drv)	\
	register_driver_macro(device,platform,drv)
#define console_platform_driver(drv)	\
	register_driver_macro(console,platform,drv)
#define mem_platform_driver(drv)	\
	register_driver_macro(mem,platform,drv)
#define fs_platform_driver(drv)	\
	register_driver_macro(fs,platform,drv)
#define late_platform_driver(drv)	\
	register_driver_macro(late,platform,drv)

int platform_device_register(struct device_d *new_device);

struct cdev_operations {
	/*! Called in response of reading from this device. Required */
	ssize_t (*read)(struct cdev*, void* buf, size_t count, loff_t offset, ulong flags);

	/*! Called in response of write to this device. Required */
	ssize_t (*write)(struct cdev*, const void* buf, size_t count, loff_t offset, ulong flags);

	int (*ioctl)(struct cdev*, int, void *);
	int (*lseek)(struct cdev*, loff_t);
	int (*open)(struct cdev*, unsigned long flags);
	int (*close)(struct cdev*);
	int (*flush)(struct cdev*);
	int (*erase)(struct cdev*, loff_t count, loff_t offset);
	int (*protect)(struct cdev*, size_t count, loff_t offset, int prot);
	int (*discard_range)(struct cdev*, loff_t count, loff_t offset);
	int (*memmap)(struct cdev*, void **map, int flags);
	int (*truncate)(struct cdev*, size_t size);
};

#define MAX_UUID_STR	sizeof("00112233-4455-6677-8899-AABBCCDDEEFF")

struct cdev {
	const struct cdev_operations *ops;
	void *priv;
	struct device_d *dev;
	struct device_node *device_node;
	struct list_head list;
	struct list_head devices_list;
	char *name; /* filename under /dev/ */
	char *partname; /* the partition name, usually the above without the
			 * device part, i.e. name = "nand0.barebox" -> partname = "barebox"
			 */
	char uuid[MAX_UUID_STR];
	loff_t offset;
	loff_t size;
	unsigned int flags;
	int open;
	struct mtd_info *mtd;
	u8 dos_partition_type;
	struct cdev *link;
	struct list_head link_entry, links;
	struct list_head partition_entry, partitions;
	struct cdev *master;
	enum filetype filetype;
};

int devfs_create(struct cdev *);
int devfs_create_link(struct cdev *, const char *name);
int devfs_remove(struct cdev *);
int cdev_find_free_index(const char *);
struct cdev *device_find_partition(struct device_d *dev, const char *name);
struct cdev *cdev_by_name(const char *filename);
struct cdev *lcdev_by_name(const char *filename);
struct cdev *cdev_readlink(struct cdev *cdev);
struct cdev *cdev_by_device_node(struct device_node *node);
struct cdev *cdev_by_partuuid(const char *partuuid);
struct cdev *cdev_by_diskuuid(const char *partuuid);
struct cdev *cdev_open_by_name(const char *name, unsigned long flags);
struct cdev *cdev_create_loop(const char *path, ulong flags, loff_t offset);
void cdev_remove_loop(struct cdev *cdev);
int cdev_open(struct cdev *, unsigned long flags);
void cdev_close(struct cdev *cdev);
int cdev_flush(struct cdev *cdev);
ssize_t cdev_read(struct cdev *cdev, void *buf, size_t count, loff_t offset, ulong flags);
ssize_t cdev_write(struct cdev *cdev, const void *buf, size_t count, loff_t offset, ulong flags);
int cdev_ioctl(struct cdev *cdev, int cmd, void *buf);
int cdev_erase(struct cdev *cdev, loff_t count, loff_t offset);
int cdev_lseek(struct cdev*, loff_t);
int cdev_protect(struct cdev*, size_t count, loff_t offset, int prot);
int cdev_discard_range(struct cdev*, loff_t count, loff_t offset);
int cdev_memmap(struct cdev*, void **map, int flags);
int cdev_truncate(struct cdev*, size_t size);
loff_t cdev_unallocated_space(struct cdev *cdev);

extern struct list_head cdev_list;
#define for_each_cdev(c) \
	list_for_each_entry(cdev, &cdev_list, list)

#define DEVFS_PARTITION_FIXED		(1U << 0)
#define DEVFS_PARTITION_READONLY	(1U << 1)
#define DEVFS_IS_CHARACTER_DEV		(1U << 3)
#define DEVFS_PARTITION_FROM_TABLE	(1U << 4)
#define DEVFS_IS_MCI_MAIN_PART_DEV	(1U << 5)

struct cdev *devfs_add_partition(const char *devname, loff_t offset,
		loff_t size, unsigned int flags, const char *name);
int devfs_del_partition(const char *name);

#ifdef CONFIG_FS_AUTOMOUNT
void cdev_create_default_automount(struct cdev *cdev);
#else
static inline void cdev_create_default_automount(struct cdev *cdev)
{
}
#endif

static inline bool cdev_is_mci_main_part_dev(struct cdev *cdev)
{
	return cdev->flags & DEVFS_IS_MCI_MAIN_PART_DEV;
}

#define DEVFS_PARTITION_APPEND		0

/**
 * struct devfs_partition - defines parameters for a single partition
 * @offset: start of partition
 * 	a negative offset requests to start the partition relative to the
 * 	device's end. DEVFS_PARTITION_APPEND (i.e. 0) means start directly at
 * 	the end of the previous partition.
 * @size: size of partition
 * 	a non-positive value requests to use a size that keeps -size free space
 * 	after the current partition. A special case of this is passing 0, which
 * 	means "until end of device".
 * @flags: flags passed to devfs_add_partition
 * @name: name passed to devfs_add_partition
 * @bbname: if non-NULL also dev_add_bb_dev() is called for the partition during
 * 	devfs_create_partitions().
 */
struct devfs_partition {
	loff_t offset;
	loff_t size;
	unsigned int flags;
	const char *name;
	const char *bbname;
};
/**
 * devfs_create_partitions - create a set of partitions for a device
 * @devname: name of the device to partition
 * @partinfo: array of partition parameters
 * 	The array is processed until an entry with .name = NULL is found.
 */
int devfs_create_partitions(const char *devname,
		const struct devfs_partition partinfo[]);

#define DRV_OF_COMPAT(compat) \
	IS_ENABLED(CONFIG_OFDEVICE) ? (compat) : NULL

/**
 * dev_get_drvdata - get driver match data associated with device
 * @dev: device instance
 * @data: pointer to void *, where match data is stored
 *
 * Returns 0 on success and error code otherwise.
 *
 * DEPRECATED: use device_get_match_data instead, which avoids
 * common pitfalls due to explicit pointer casts
 */
int dev_get_drvdata(struct device_d *dev, const void **data);

/**
 * device_get_match_data - get driver match data associated with device
 * @dev: device instance
 *
 * Returns match data on success and NULL otherwise
 */
const void *device_get_match_data(struct device_d *dev);

int device_match_of_modalias(struct device_d *dev, struct driver_d *drv);

struct device_d *device_find_child(struct device_d *parent, void *data,
				 int (*match)(struct device_d *dev, void *data));

static inline struct device_node *dev_of_node(struct device_d *dev)
{
	return IS_ENABLED(CONFIG_OFDEVICE) ? dev->device_node : NULL;
}

#endif /* DRIVER_H */
