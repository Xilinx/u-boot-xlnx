/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * composite.h -- framework for usb gadgets which are composite devices
 *
 * Copyright (C) 2006-2008 David Brownell
 */

#ifndef	__LINUX_USB_COMPOSITE_H
#define	__LINUX_USB_COMPOSITE_H

/*
 * This framework is an optional layer on top of the USB Gadget interface,
 * making it easier to build (a) Composite devices, supporting multiple
 * functions within any single configuration, and (b) Multi-configuration
 * devices, also supporting multiple functions but without necessarily
 * having more than one function per configuration.
 *
 * Example:  a device with a single configuration supporting both network
 * link and mass storage functions is a composite device.  Those functions
 * might alternatively be packaged in individual configurations, but in
 * the composite model the host can use both functions at the same time.
 */

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/bitmap.h>

/*
 * USB function drivers should return USB_GADGET_DELAYED_STATUS if they
 * wish to delay the data/status stages of the control transfer till they
 * are ready. The control transfer will then be kept from completing till
 * all the function drivers that requested for USB_GADGET_DELAYED_STAUS
 * invoke usb_composite_setup_continue().
 */
#define	USB_GADGET_DELAYED_STATUS	0x7fff /* Impossibly large value */

struct usb_configuration;

/**
 * struct usb_os_desc_ext_prop - describes one "Extended Property"
 * @entry: used to keep a list of extended properties
 * @type: Extended Property type
 * @name_len: Extended Property unicode name length, including terminating '\0'
 * @name: Extended Property name
 * @data_len: Length of Extended Property blob (for unicode store double len)
 * @data: Extended Property blob
 */
struct usb_os_desc_ext_prop {
	struct list_head	entry;
	u8			type;
	int			name_len;
	char			*name;
	int			data_len;
	char			*data;
};

/**
 * struct usb_os_desc - describes OS descriptors associated with one interface
 * @ext_compat_id: 16 bytes of "Compatible ID" and "Subcompatible ID"
 * @ext_prop: Extended Properties list
 * @ext_prop_len: Total length of Extended Properties blobs
 * @ext_prop_count: Number of Extended Properties
 */
struct usb_os_desc {
	char			*ext_compat_id;
	struct list_head	ext_prop;
	int			ext_prop_len;
	int			ext_prop_count;
};

/**
 * struct usb_os_desc_table - describes OS descriptors associated with one
 * interface of a usb_function
 * @if_id: Interface id
 * @os_desc: "Extended Compatibility ID" and "Extended Properties" of the
 *	interface
 *
 * Each interface can have at most one "Extended Compatibility ID" and a
 * number of "Extended Properties".
 */
struct usb_os_desc_table {
	int			if_id;
	struct usb_os_desc	*os_desc;
};

/**
 * struct usb_function - describes one function of a configuration
 * @name: For diagnostics, identifies the function.
 * @strings: tables of strings, keyed by identifiers assigned during bind()
 *	and by language IDs provided in control requests
 * @descriptors: Table of full (or low) speed descriptors, using interface and
 *	string identifiers assigned during @bind().  If this pointer is null,
 *	the function will not be available at full speed (or at low speed).
 * @hs_descriptors: Table of high speed descriptors, using interface and
 *	string identifiers assigned during @bind().  If this pointer is null,
 *	the function will not be available at high speed.
 * @config: assigned when @usb_add_function() is called; this is the
 *	configuration with which this function is associated.
 * @os_desc_table: Table of (interface id, os descriptors) pairs. The function
 *	can expose more than one interface. If an interface is a member of
 *	an IAD, only the first interface of IAD has its entry in the table.
 * @os_desc_n: Number of entries in os_desc_table
 * @bind: Before the gadget can register, all of its functions bind() to the
 *	available resources including string and interface identifiers used
 *	in interface or class descriptors; endpoints; I/O buffers; and so on.
 * @unbind: Reverses @bind; called as a side effect of unregistering the
 *	driver which added this function.
 * @set_alt: (REQUIRED) Reconfigures altsettings; function drivers may
 *	initialize usb_ep.driver data at this time (when it is used).
 *	Note that setting an interface to its current altsetting resets
 *	interface state, and that all interfaces have a disabled state.
 * @get_alt: Returns the active altsetting.  If this is not provided,
 *	then only altsetting zero is supported.
 * @disable: (REQUIRED) Indicates the function should be disabled.  Reasons
 *	include host resetting or reconfiguring the gadget, and disconnection.
 * @setup: Used for interface-specific control requests.
 * @suspend: Notifies functions when the host stops sending USB traffic.
 * @resume: Notifies functions when the host restarts USB traffic.
 *
 * A single USB function uses one or more interfaces, and should in most
 * cases support operation at both full and high speeds.  Each function is
 * associated by @usb_add_function() with a one configuration; that function
 * causes @bind() to be called so resources can be allocated as part of
 * setting up a gadget driver.  Those resources include endpoints, which
 * should be allocated using @usb_ep_autoconfig().
 *
 * To support dual speed operation, a function driver provides descriptors
 * for both high and full speed operation.  Except in rare cases that don't
 * involve bulk endpoints, each speed needs different endpoint descriptors.
 *
 * Function drivers choose their own strategies for managing instance data.
 * The simplest strategy just declares it "static', which means the function
 * can only be activated once.  If the function needs to be exposed in more
 * than one configuration at a given speed, it needs to support multiple
 * usb_function structures (one for each configuration).
 *
 * A more complex strategy might encapsulate a @usb_function structure inside
 * a driver-specific instance structure to allows multiple activations.  An
 * example of multiple activations might be a CDC ACM function that supports
 * two or more distinct instances within the same configuration, providing
 * several independent logical data links to a USB host.
 */
struct usb_function {
	const char			*name;
	struct usb_gadget_strings	**strings;
	struct usb_descriptor_header	**descriptors;
	struct usb_descriptor_header	**hs_descriptors;
	struct usb_descriptor_header    **ss_descriptors;

	struct usb_configuration	*config;

	struct usb_os_desc_table	*os_desc_table;
	unsigned			os_desc_n;

	/* REVISIT:  bind() functions can be marked __init, which
	 * makes trouble for section mismatch analysis.  See if
	 * we can't restructure things to avoid mismatching.
	 * Related:  unbind() may kfree() but bind() won't...
	 */

	/* configuration management:  bind/unbind */
	int			(*bind)(struct usb_configuration *,
					struct usb_function *);
	void			(*unbind)(struct usb_configuration *,
					struct usb_function *);

	/* runtime state management */
	int			(*set_alt)(struct usb_function *,
					unsigned interface, unsigned alt);
	int			(*get_alt)(struct usb_function *,
					unsigned interface);
	void			(*disable)(struct usb_function *);
	int			(*setup)(struct usb_function *,
					const struct usb_ctrlrequest *);
	void			(*suspend)(struct usb_function *);
	void			(*resume)(struct usb_function *);

	/* private: */
	/* internals */
	struct list_head		list;
	DECLARE_BITMAP(endpoints, 32);
};

int usb_add_function(struct usb_configuration *, struct usb_function *);

int usb_function_deactivate(struct usb_function *);
int usb_function_activate(struct usb_function *);

int usb_interface_id(struct usb_configuration *, struct usb_function *);

/**
 * ep_choose - select descriptor endpoint at current device speed
 * @g: gadget, connected and running at some speed
 * @hs: descriptor to use for high speed operation
 * @fs: descriptor to use for full or low speed operation
 */
static inline struct usb_endpoint_descriptor *
ep_choose(struct usb_gadget *g, struct usb_endpoint_descriptor *hs,
		struct usb_endpoint_descriptor *fs)
{
	if (gadget_is_dualspeed(g) && g->speed == USB_SPEED_HIGH)
		return hs;
	return fs;
}

#define	MAX_CONFIG_INTERFACES		16	/* arbitrary; max 255 */

/**
 * struct usb_configuration - represents one gadget configuration
 * @label: For diagnostics, describes the configuration.
 * @strings: Tables of strings, keyed by identifiers assigned during @bind()
 *	and by language IDs provided in control requests.
 * @descriptors: Table of descriptors preceding all function descriptors.
 *	Examples include OTG and vendor-specific descriptors.
 * @bind: Called from @usb_add_config() to allocate resources unique to this
 *	configuration and to call @usb_add_function() for each function used.
 * @unbind: Reverses @bind; called as a side effect of unregistering the
 *	driver which added this configuration.
 * @setup: Used to delegate control requests that aren't handled by standard
 *	device infrastructure or directed at a specific interface.
 * @bConfigurationValue: Copied into configuration descriptor.
 * @iConfiguration: Copied into configuration descriptor.
 * @bmAttributes: Copied into configuration descriptor.
 * @bMaxPower: Copied into configuration descriptor.
 * @cdev: assigned by @usb_add_config() before calling @bind(); this is
 *	the device associated with this configuration.
 *
 * Configurations are building blocks for gadget drivers structured around
 * function drivers.  Simple USB gadgets require only one function and one
 * configuration, and handle dual-speed hardware by always providing the same
 * functionality.  Slightly more complex gadgets may have more than one
 * single-function configuration at a given speed; or have configurations
 * that only work at one speed.
 *
 * Composite devices are, by definition, ones with configurations which
 * include more than one function.
 *
 * The lifecycle of a usb_configuration includes allocation, initialization
 * of the fields described above, and calling @usb_add_config() to set up
 * internal data and bind it to a specific device.  The configuration's
 * @bind() method is then used to initialize all the functions and then
 * call @usb_add_function() for them.
 *
 * Those functions would normally be independant of each other, but that's
 * not mandatory.  CDC WMC devices are an example where functions often
 * depend on other functions, with some functions subsidiary to others.
 * Such interdependency may be managed in any way, so long as all of the
 * descriptors complete by the time the composite driver returns from
 * its bind() routine.
 */
struct usb_configuration {
	const char			*label;
	struct usb_gadget_strings	**strings;
	const struct usb_descriptor_header **descriptors;

	/* REVISIT:  bind() functions can be marked __init, which
	 * makes trouble for section mismatch analysis.  See if
	 * we can't restructure things to avoid mismatching...
	 */

	/* configuration management:  bind/unbind */
	int			(*bind)(struct usb_configuration *);
	void			(*unbind)(struct usb_configuration *);
	int			(*setup)(struct usb_configuration *,
					const struct usb_ctrlrequest *);

	/* fields in the config descriptor */
	u8			bConfigurationValue;
	u8			iConfiguration;
	u8			bmAttributes;
	u8			bMaxPower;

	struct usb_composite_dev	*cdev;

	/* private: */
	/* internals */
	struct list_head	list;
	struct list_head	functions;
	u8			next_interface_id;
	unsigned		highspeed:1;
	unsigned		fullspeed:1;
	unsigned		superspeed:1;
	struct usb_function	*interface[MAX_CONFIG_INTERFACES];
};

int usb_add_config(struct usb_composite_dev *,
		struct usb_configuration *);

/**
 * struct usb_composite_driver - groups configurations into a gadget
 * @name: For diagnostics, identifies the driver.
 * @dev: Template descriptor for the device, including default device
 *	identifiers.
 * @strings: tables of strings, keyed by identifiers assigned during bind()
 *	and language IDs provided in control requests
 * @max_speed: Highest speed the driver supports.
 * @bind: (REQUIRED) Used to allocate resources that are shared across the
 *	whole device, such as string IDs, and add its configurations using
 *	@usb_add_config().  This may fail by returning a negative errno
 *	value; it should return zero on successful initialization.
 * @unbind: Reverses @bind(); called as a side effect of unregistering
 *	this driver.
 * @disconnect: optional driver disconnect method
 * @suspend: Notifies when the host stops sending USB traffic,
 *	after function notifications
 * @resume: Notifies configuration when the host restarts USB traffic,
 *	before function notifications
 *
 * Devices default to reporting self powered operation.  Devices which rely
 * on bus powered operation should report this in their @bind() method.
 *
 * Before returning from @bind, various fields in the template descriptor
 * may be overridden.  These include the idVendor/idProduct/bcdDevice values
 * normally to bind the appropriate host side driver, and the three strings
 * (iManufacturer, iProduct, iSerialNumber) normally used to provide user
 * meaningful device identifiers.  (The strings will not be defined unless
 * they are defined in @dev and @strings.)  The correct ep0 maxpacket size
 * is also reported, as defined by the underlying controller driver.
 */
struct usb_composite_driver {
	const char				*name;
	const struct usb_device_descriptor	*dev;
	struct usb_gadget_strings		**strings;
	enum usb_device_speed			max_speed;

	/* REVISIT:  bind() functions can be marked __init, which
	 * makes trouble for section mismatch analysis.  See if
	 * we can't restructure things to avoid mismatching...
	 */

	int			(*bind)(struct usb_composite_dev *);
	int			(*unbind)(struct usb_composite_dev *);

	void			(*disconnect)(struct usb_composite_dev *);

	/* global suspend hooks */
	void			(*suspend)(struct usb_composite_dev *);
	void			(*resume)(struct usb_composite_dev *);
};

extern int usb_composite_register(struct usb_composite_driver *);
extern void usb_composite_unregister(struct usb_composite_driver *);

#define OS_STRING_QW_SIGN_LEN		14
#define OS_STRING_IDX			0xEE

/**
 * struct usb_composite_device - represents one composite usb gadget
 * @gadget: read-only, abstracts the gadget's usb peripheral controller
 * @req: used for control responses; buffer is pre-allocated
 * @bufsiz: size of buffer pre-allocated in @req
 * @os_desc_req: used for OS descriptors responses; buffer is pre-allocated
 * @config: the currently active configuration
 * @qw_sign: qwSignature part of the OS string
 * @b_vendor_code: bMS_VendorCode part of the OS string
 * @use_os_string: false by default, interested gadgets set it
 * @os_desc_config: the configuration to be used with OS descriptors
 *
 * One of these devices is allocated and initialized before the
 * associated device driver's bind() is called.
 *
 * OPEN ISSUE:  it appears that some WUSB devices will need to be
 * built by combining a normal (wired) gadget with a wireless one.
 * This revision of the gadget framework should probably try to make
 * sure doing that won't hurt too much.
 *
 * One notion for how to handle Wireless USB devices involves:
 * (a) a second gadget here, discovery mechanism TBD, but likely
 *     needing separate "register/unregister WUSB gadget" calls;
 * (b) updates to usb_gadget to include flags "is it wireless",
 *     "is it wired", plus (presumably in a wrapper structure)
 *     bandgroup and PHY info;
 * (c) presumably a wireless_ep wrapping a usb_ep, and reporting
 *     wireless-specific parameters like maxburst and maxsequence;
 * (d) configurations that are specific to wireless links;
 * (e) function drivers that understand wireless configs and will
 *     support wireless for (additional) function instances;
 * (f) a function to support association setup (like CBAF), not
 *     necessarily requiring a wireless adapter;
 * (g) composite device setup that can create one or more wireless
 *     configs, including appropriate association setup support;
 * (h) more, TBD.
 */
struct usb_composite_dev {
	struct usb_gadget		*gadget;
	struct usb_request		*req;
	unsigned			bufsiz;

	struct usb_configuration	*config;

	/* OS String is a custom (yet popular) extension to the USB standard. */
	u8				qw_sign[OS_STRING_QW_SIGN_LEN];
	u8				b_vendor_code;
	struct usb_configuration	*os_desc_config;
	unsigned int			use_os_string:1;

	/* private: */
	/* internals */
	unsigned int			suspended:1;
	struct usb_device_descriptor __aligned(CONFIG_SYS_CACHELINE_SIZE) desc;
	struct list_head		configs;
	struct usb_composite_driver	*driver;
	u8				next_string_id;

	/* the gadget driver won't enable the data pullup
	 * while the deactivation count is nonzero.
	 */
	unsigned			deactivations;
};

extern int usb_string_id(struct usb_composite_dev *c);
extern int usb_string_ids_tab(struct usb_composite_dev *c,
			      struct usb_string *str);
extern int usb_string_ids_n(struct usb_composite_dev *c, unsigned n);

#endif	/* __LINUX_USB_COMPOSITE_H */
