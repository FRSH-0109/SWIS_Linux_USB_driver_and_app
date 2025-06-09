#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/types.h>

#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>

static int vendor_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void vendor_disconnect(struct usb_interface *interface);

/* Meta information*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kamil Kośnik");
MODULE_DESCRIPTION("A simple USB driver for a custom USB HID");
MODULE_VERSION("0.1");

/* Define these values to match your devices */
#define USB_SKEL_VENDOR_ID	0x0483
#define USB_SKEL_PRODUCT_ID	0xd431

#define EP_IN  0x81  // IRQ IN endpoint address
#define EP_OUT 0x01  // IRQ OUT endpoint address
#define MAX_PKT_SIZE 64

struct usb_vendor {
    struct usb_device *udev;
    struct usb_interface *interface;
    unsigned char *irq_in_buffer;
    size_t irq_in_size;
    __u8 irq_in_endpointAddr;
    __u8 irq_out_endpointAddr;
    struct mutex io_mutex;
};

static struct usb_class_driver vendor_class = {
    .name = "usb/vendor%d",
    .fops = NULL, // Set below
    .minor_base = 192,  //0–63	Reserved for core USB drivers, 192–255	Reserved for custom USB drivers via usb_register_dev()
};

/* table of devices that work with this driver */
static const struct usb_device_id vendor_usb_table[] = {
	{ USB_DEVICE(USB_SKEL_VENDOR_ID, USB_SKEL_PRODUCT_ID) },
    // {USB_DEVICE_INFO(calss, subclass, protocol) }, /* Example for class, subclass, protocol */)}
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, vendor_usb_table);

static struct usb_driver vendor_usb_driver = {
    .name = "vendor_usb_driver",
    .id_table = vendor_usb_table,
    .probe = vendor_probe,
    .disconnect = vendor_disconnect,
};

static int vendor_open(struct inode *inode, struct file *file)
{
    struct usb_vendor *dev;
    struct usb_interface *interface;
    int subminor = iminor(inode);

    interface = usb_find_interface(&vendor_usb_driver, subminor);
    if (!interface)
        return -ENODEV;

    dev = usb_get_intfdata(interface);
    if (!dev)
        return -ENODEV;

    file->private_data = dev;
    pr_info("VENDOR USB device open: minor ID: %d\n", subminor);
    return 0;
}

static int vendor_release(struct inode *inode, struct file *file)
{
    pr_info("VENDOR USB device release\n");
    return 0;
}

static ssize_t vendor_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
    int actual_length = 0;
    pr_info("VENDOR USB device read: length: %d\n", actual_length);
    struct usb_vendor *dev = file->private_data;
    int retval;
    
    if (!dev) {
        pr_info("VENDOR USB device read: errr 1\n");
        return -ENODEV;
    }

    if (count > MAX_PKT_SIZE)
        count = MAX_PKT_SIZE;

    retval = usb_interrupt_msg(dev->udev,
                          usb_rcvintpipe(dev->udev, dev->irq_in_endpointAddr),
                          dev->irq_in_buffer,
                          count,
                          &actual_length,
                          1000);

    if (retval) {
        pr_info("VENDOR USB device read: errr 2\n");
        return retval;
    }


    if (copy_to_user(buffer, dev->irq_in_buffer, actual_length)) {
        pr_info("VENDOR USB device read: errr 2\n");
        return -EFAULT;
    }
    
    pr_info("VENDOR USB device read: length: %d\n", actual_length);
    return actual_length;
}

static ssize_t vendor_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos)
{
    int wrote_cnt = min((size_t)MAX_PKT_SIZE, count);
    pr_info("VENDOR USB device write: length: %d\n", wrote_cnt);
    struct usb_vendor *dev = file->private_data;
    int retval;
    char *buf;

    if (!dev)
        return -ENODEV;

    if (wrote_cnt == 0)
        return 0;

    buf = kmalloc(MAX_PKT_SIZE, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, user_buffer, wrote_cnt)) {
        kfree(buf);
        return -EFAULT;
    }

    retval = usb_interrupt_msg(dev->udev,
                          usb_sndintpipe(dev->udev, dev->irq_out_endpointAddr),
                          buf,
                          wrote_cnt,
                          NULL,
                          1000);
    
    kfree(buf);
    pr_info("VENDOR USB device write: length: %d\n", wrote_cnt);
    return retval ? retval : wrote_cnt;
}

static const struct file_operations vendor_fops = {
    .owner = THIS_MODULE,
    .open = vendor_open,
    .release = vendor_release,
    .read = vendor_read,
    .write = vendor_write,
};

static int vendor_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    pr_info("USB device probed: Vendor ID: %04x, Product ID: %04x\n", id->idVendor, id->idProduct);
    struct usb_endpoint_descriptor *endpoint;
    struct usb_host_interface *iface_desc;
    struct usb_vendor *dev;
    int i;

    dev = kzalloc(sizeof(struct usb_vendor), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->udev = usb_get_dev(interface_to_usbdev(interface));
    dev->interface = interface;

    iface_desc = interface->cur_altsetting;
    for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
        endpoint = &iface_desc->endpoint[i].desc;

        if (!dev->irq_in_endpointAddr &&
            usb_endpoint_is_int_in(endpoint)) {
            dev->irq_in_size = endpoint->wMaxPacketSize;
            dev->irq_in_endpointAddr = endpoint->bEndpointAddress;
            dev->irq_in_buffer = kmalloc(dev->irq_in_size, GFP_KERNEL);
        }

        if (!dev->irq_out_endpointAddr &&
            usb_endpoint_is_int_out(endpoint)) {
            dev->irq_out_endpointAddr = endpoint->bEndpointAddress;
        }
    }

    if (!(dev->irq_in_endpointAddr && dev->irq_out_endpointAddr)) {
        kfree(dev);
        return -ENODEV;
    }

    usb_set_intfdata(interface, dev);
    mutex_init(&dev->io_mutex);

    vendor_class.fops = &vendor_fops;
    return usb_register_dev(interface, &vendor_class);
}

static void vendor_disconnect(struct usb_interface *interface) {
    pr_info("USB device disconnected \n");
    struct usb_vendor *dev;
    dev = usb_get_intfdata(interface);
    usb_deregister_dev(interface, &vendor_class);
    kfree(dev->irq_in_buffer);
    kfree(dev);
}

static int __init my_init(void) {
    int ret;
    ret = usb_register(&vendor_usb_driver);
    if (ret) {
        pr_err("USB custom driver initialize failed with error %d\n", ret);
        return -ret;
    }
    pr_info("USB custom driver initialized\n");
    return 0;
}

static void __exit my_exit(void) {
    usb_deregister(&vendor_usb_driver);
    pr_info("USB custom driver exited\n");
}


module_init(my_init);
module_exit(my_exit);