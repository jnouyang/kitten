/*
 * HobbesIO Syscall Forwarding Support
 *
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 *
 */
#include <lwk/kfs.h>
#include <lwk/driver.h>
#include <lwk/hio/hio_ioctl.h>


static int
hio_open(struct inode *inode, struct file *file) {
    return 0;
}


static int
hio_release(struct inode * inodep, struct file  * filp) {
    return 0;
}


/*
 * User ioctl to the HIO driver. 
 * Only 64-bit user applications are supported.
 */
static long
hio_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    long ret;

    switch (cmd) {
        case HIO_CMD_TEST: {
        }
        default:
            break;
    }
    return -ENOIOCTLCMD;
}

static struct kfs_fops 
hio_fops = {
    .open           = hio_open,
    .release        = hio_release,
    .unlocked_ioctl = hio_ioctl,
    .compat_ioctl   = hio_ioctl
};


static int
hio_init(void) {
    int ret = 0;

    kfs_create("/dev/hio",
        NULL,
        &hio_fops,
        0777,
        NULL,
        0);

    printk("HIO loaded\n");
    return ret;
}


DRIVER_INIT("kfs", hio_init);
//DRIVER_EXIT(hio_exit);
