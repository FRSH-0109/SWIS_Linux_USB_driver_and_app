#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/types.h>

#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/ioctl.h>

#define WR_PERIOD _IOW('c', 1, uint32_t*)
// #define RD_PERIOD _IOR('c', 2, uint32_t*)

static int vendor_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void vendor_disconnect(struct usb_interface *interface);

static void vendor_out_timer_func(struct timer_list *t);
static void vendor_out_work_func(struct work_struct *work);
static void interrupt_in_callback(struct urb *urb);

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

const char CMD_SHTC3_READ[] = "SHTC3 READ";

struct usb_vendor {
    struct usb_device *udev;
    struct usb_interface *interface;
    unsigned char *irq_in_buffer;
    size_t irq_in_size;
    __u8 irq_in_endpointAddr;
    __u8 irq_out_endpointAddr;
    int bInterval_out_endpoint;

    unsigned char cmd[64];

    // For interrupt transfers
    unsigned char *int_in_buffer;
    struct urb *int_in_urb;
    size_t int_in_size;

    // Shared data for userspace
    unsigned char latest_data[MAX_PKT_SIZE];
    size_t latest_length;
    struct mutex data_lock;

    /* Data request staff sent to USB device periodically*/
    struct timer_list out_timer;
    struct work_struct out_work;
    uint32_t POLLING_INTERVAL_MS; // Polling interval in ms
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
    memset(dev->latest_data, 0, sizeof(dev->latest_data)); // Clear latest
    dev->latest_length = 0; // Reset latest data length
    return 0;
}

static int vendor_release(struct inode *inode, struct file *file)
{
    pr_info("VENDOR USB device release\n");
    return 0;
}

static ssize_t vendor_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
    struct usb_vendor *dev = file->private_data;

    if (!dev) {
        pr_info("VENDOR USB device read: errr 1\n");
        return -ENODEV;
    }

    if (count > MAX_PKT_SIZE)
        count = MAX_PKT_SIZE;

    mutex_lock(&dev->data_lock);
    if (dev->latest_length == 0) {
        mutex_unlock(&dev->data_lock);
        return 0; // No new data
    }
    else if (count > dev->latest_length) {
        count = dev->latest_length; // Limit to available data length
    }

    if (copy_to_user(buffer, dev->latest_data, count)) {
        mutex_unlock(&dev->data_lock);
        return -EFAULT;
    }

    dev->latest_length = 0; // Consume data after read
    mutex_unlock(&dev->data_lock);

    pr_info("VENDOR USB device read: length: %d\n", (int)count);
    return count;
}

static ssize_t vendor_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos)
{
    int wrote_cnt = min((size_t)MAX_PKT_SIZE, count);
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

static long vendor_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct usb_vendor *dev = file->private_data;
    switch(cmd) {
        case WR_PERIOD:
            if (copy_from_user(&dev->POLLING_INTERVAL_MS, (uint32_t*) arg, sizeof(dev->POLLING_INTERVAL_MS)))
            {
                pr_err("Data write : Err!\n");
            }
            pr_info("Value = %d\n", dev->POLLING_INTERVAL_MS);
            break;
        // case RD_PERIOD:
        //     if (copy_to_user((uint32_t*) arg, &dev->POLLING_INTERVAL_MS, sizeof(dev->POLLING_INTERVAL_MS)))
        //     {
        //         pr_err("Data Read: Err!\n");
        //     }
        //     break;
        default:
            pr_info("Invalid ioctl command!");
            break;
    }
    return 0;
}

static const struct file_operations vendor_fops = {
    .owner = THIS_MODULE,
    .open = vendor_open,
    .release = vendor_release,
    .read = vendor_read,
    .write = vendor_write,
    .unlocked_ioctl = vendor_ioctl
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
            dev->int_in_size = endpoint->wMaxPacketSize;
            dev->irq_in_endpointAddr = endpoint->bEndpointAddress;
            dev->irq_in_buffer = kmalloc(dev->irq_in_size, GFP_KERNEL);
        }

        if (!dev->irq_out_endpointAddr &&
            usb_endpoint_is_int_out(endpoint)) {
            dev->irq_out_endpointAddr = endpoint->bEndpointAddress;
            dev->bInterval_out_endpoint = endpoint->bInterval; // Polling interval in ms
        }
    }

    if (!(dev->irq_in_endpointAddr && dev->irq_out_endpointAddr)) {
        kfree(dev);
        return -ENODEV;
    }

    usb_set_intfdata(interface, dev);

    mutex_init(&dev->data_lock);
    dev->int_in_buffer = kmalloc(dev->int_in_size, GFP_KERNEL);
    dev->int_in_urb = usb_alloc_urb(0, GFP_KERNEL);

    usb_fill_int_urb(dev->int_in_urb,
                    dev->udev,
                    usb_rcvintpipe(dev->udev, dev->irq_in_endpointAddr),
                    dev->int_in_buffer,
                    dev->int_in_size,
                    interrupt_in_callback,
                    dev,
                    dev->bInterval_out_endpoint);        // From descriptor: polling interval in ms

    usb_submit_urb(dev->int_in_urb, GFP_KERNEL);

    vendor_class.fops = &vendor_fops;

    /* Data polling from device */
    // dev->POLLING_INTERVAL_MS = 1000;    // Polling interval in ms
    // INIT_WORK(&dev->out_work, vendor_out_work_func);
    // timer_setup(&dev->out_timer, vendor_out_timer_func, 0);
    // // Kick off first timer
    // mod_timer(&dev->out_timer, jiffies + msecs_to_jiffies(dev->POLLING_INTERVAL_MS));

    return usb_register_dev(interface, &vendor_class);
}

static void vendor_disconnect(struct usb_interface *interface) {
    pr_info("USB device disconnected \n");
    struct usb_vendor *dev;
    dev = usb_get_intfdata(interface);
    usb_deregister_dev(interface, &vendor_class);
    kfree(dev->irq_in_buffer);
    usb_kill_urb(dev->int_in_urb);
    usb_free_urb(dev->int_in_urb);
    kfree(dev->int_in_buffer);
    del_timer_sync(&dev->out_timer);
    cancel_work_sync(&dev->out_work);
    mutex_destroy(&dev->data_lock);
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

/* Fucntion for handling data request timer callback */
static void vendor_out_timer_func(struct timer_list *t)
{
    struct usb_vendor *dev = from_timer(dev, t, out_timer);

    pr_warn("TIMERRRRRR\n");
    schedule_work(&dev->out_work); // schedule OUT transfer

    // restart timer
    mod_timer(&dev->out_timer, jiffies + msecs_to_jiffies(dev->POLLING_INTERVAL_MS));
}

/* Worker function for data request sending*/
static void vendor_out_work_func(struct work_struct *work)
{
    struct usb_vendor *dev = container_of(work, struct usb_vendor, out_work);
    memcpy(dev->cmd, CMD_SHTC3_READ, sizeof(CMD_SHTC3_READ));
    int retval;

    pr_warn("WOKRERRRR\n");
    retval = usb_interrupt_msg(dev->udev,
                  usb_sndintpipe(dev->udev, dev->irq_out_endpointAddr),
                  dev->cmd, sizeof(dev->cmd), NULL, 1000);

    if (retval)
        pr_err("USB OUT transfer failed: %d\n", retval);
    else
        pr_debug("USB OUT transfer sent.\n");
}

// callback function for interrupt IN URB
static void interrupt_in_callback(struct urb *urb)
{
    struct usb_vendor *dev = urb->context;

    if (urb->status == 0) {
        pr_info("USB INT POLL: %d bytes: %*ph | ASCII: %.*s\n",
                urb->actual_length,
                urb->actual_length, dev->int_in_buffer,
                urb->actual_length, dev->int_in_buffer);
        mutex_lock(&dev->data_lock);
        if (urb->actual_length > MAX_PKT_SIZE) {
            pr_warn("Received data exceeds buffer size, truncating\n");
            urb->actual_length = MAX_PKT_SIZE;
        }
        memcpy(dev->latest_data, dev->int_in_buffer, urb->actual_length);
        dev->latest_length = urb->actual_length;
        mutex_unlock(&dev->data_lock);
    } else {
        pr_err("INT URB error: %d\n", urb->status);
    }

    // Re-submit URB to continue polling
    usb_submit_urb(dev->int_in_urb, GFP_ATOMIC);
}