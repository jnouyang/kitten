/*
 * HobbesIO Syscall Forwarding Support
 *
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 *
 * How HIO work
 * 1) launch Kitten instance, the HIO module creates /dev/hio, reloads syscalls
 * 2) launch HIO module on the I/O domain, that exports memory region named "IO
 *    domain name" via xpmem
 * 3) issue ioctl to /dev/hio to attach to the xpmem region
 * 4) launch Kitten App
 *      4.1) launch stub process on IO domain with "stub_id"
 *      4.2) launch Kitten App with the "stub_id"
 *
 */
#include <arch/vsyscall.h>
#include <arch/unistd.h>
#include <lwk/kfs.h>
#include <lwk/driver.h>
#include <lwk/hio/hio_ioctl.h>

#include "hio.h"

static struct hio_engine *engine = NULL;

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
 */
static long
hio_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    long ret;

    switch (cmd) {
        case HIO_CMD_ATTACH: 
            {
                engine = NULL;

                break;
            }
        default:
            ret = -ENOIOCTLCMD;
            break;
    }
    return ret;
}

static struct kfs_fops 
hio_fops = {
    .open           = hio_open,
    .release        = hio_release,
    .unlocked_ioctl = hio_ioctl,
    .compat_ioctl   = hio_ioctl
};

static int
forward_syscall(
	uint64_t    arg0,
	uint64_t    arg1,
	uint64_t    arg2,
	uint64_t    arg3,
	uint64_t    arg4
) {
    int ret = 0;

    printk(KERN_INFO "Forward syscall\n");

    if (engine == NULL) {
        printk(KERN_ERR "HIO engine is not initialized when forwarding syscall\n");
        return -1;
    }

    return ret;
}


static int
hio_module_init(void) {
    int ret = 0;

    printk(KERN_INFO "Load Hobbes IO (HIO) module\n");

    kfs_create("/dev/hio",
        NULL,
        &hio_fops,
        0777,
        NULL,
        0);

    syscall_register( __NR_socket, (syscall_ptr_t) forward_syscall );
    syscall_register( __NR_bind, (syscall_ptr_t) forward_syscall );
    syscall_register( __NR_connect, (syscall_ptr_t) forward_syscall );
    syscall_register( __NR_accept, (syscall_ptr_t) forward_syscall );
    syscall_register( __NR_listen, (syscall_ptr_t) forward_syscall );
    syscall_register( __NR_select, (syscall_ptr_t) forward_syscall );
    syscall_register( __NR_setsockopt, (syscall_ptr_t) forward_syscall );
    syscall_register( __NR_getsockopt, (syscall_ptr_t) forward_syscall );

    //syscall_register( __NR_sendto, (syscall_ptr_t) forward_syscall );
    //syscall_register( __NR_sendmsg, (syscall_ptr_t) forward_syscall );
    //syscall_register( __NR_recvfrom, (syscall_ptr_t) forward_syscall );
    //syscall_register( __NR_recvmsg, (syscall_ptr_t) forward_syscall );

    return ret;
}


DRIVER_INIT("module", hio_module_init);
//DRIVER_EXIT(hio_exit);
