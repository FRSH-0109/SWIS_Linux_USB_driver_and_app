#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>

/* Meta information*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kamil Kośnik");
MODULE_DESCRIPTION("A simple USB driver for a custom USB HID");
MODULE_VERSION("0.1");

/* Define these values to match your devices */
#define USB_SKEL_VENDOR_ID	0x0483
#define USB_SKEL_PRODUCT_ID	0xd431

/* table of devices that work with this driver */
static const struct usb_device_id my_usb_table[] = {
	{ USB_DEVICE(USB_SKEL_VENDOR_ID, USB_SKEL_PRODUCT_ID) },
    // {USB_DEVICE_INFO(calss, subclass, protocol) }, /* Example for class, subclass, protocol */)}
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, my_usb_table);

static int my_usb_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    pr_info("USB device probed: Vendor ID: %04x, Product ID: %04x\n", id->idVendor, id->idProduct);
    return 0;
}

static void my_usb_disconnect(struct usb_interface *interface) {
    pr_info("USB device disconnected \n");
}

static struct usb_driver my_usb_driver = {
    .name = "my_usb_driver",
    .id_table = my_usb_table,
    .probe = my_usb_probe,
    .disconnect = my_usb_disconnect,
};

static int __init my_init(void) {
    int ret;
    ret = usb_register(&my_usb_driver);
    if (ret) {
        pr_err("USB custom driver initialize failed with error %d\n", ret);
        return -ret;
    }
    pr_info("USB custom driver initialized\n");
    return 0;
}

static void __exit my_exit(void) {
    usb_deregister(&my_usb_driver);
    pr_info("USB custom driver exited\n");
}


module_init(my_init);
module_exit(my_exit);