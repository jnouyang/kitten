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
#include <lwk/waitq.h>
#include <lwk/hio/hio_ioctl.h>

#include "hio.h"

#define STUB_ID 123

static int forward_syscall(int syscall_nr, uint64_t arg0, uint64_t arg1, 
        uint64_t arg2, uint64_t arg3, uint64_t arg4); 

static struct hio_engine *engine = NULL;

struct pending_syscalls {
    int stub_id;
    waitq_t waitq;

    int ret_val;
    int is_pending;
};

static struct pending_syscalls pending_syscall_array[MAX_STUBS];

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
hio_socket(uint64_t arg0, uint64_t arg1,
	uint64_t arg2, uint64_t arg3,
	uint64_t arg4) {
    return forward_syscall(__NR_socket, arg0, arg1, arg2, arg3, arg4); 
}

static int
hio_bind(uint64_t arg0, uint64_t arg1,
	uint64_t arg2, uint64_t arg3,
	uint64_t arg4) {
    return forward_syscall(__NR_bind, arg0, arg1, arg2, arg3, arg4); 
}

static int
hio_connect(uint64_t arg0, uint64_t arg1,
	uint64_t arg2, uint64_t arg3,
	uint64_t arg4) {
    return forward_syscall(__NR_connect, arg0, arg1, arg2, arg3, arg4); 
}

static int
hio_accept(uint64_t arg0, uint64_t arg1,
	uint64_t arg2, uint64_t arg3,
	uint64_t arg4) {
    return forward_syscall(__NR_accept, arg0, arg1, arg2, arg3, arg4); 
}

static int
hio_listen(uint64_t arg0, uint64_t arg1,
	uint64_t arg2, uint64_t arg3,
	uint64_t arg4) {
    return forward_syscall(__NR_listen, arg0, arg1, arg2, arg3, arg4); 
}

static int
hio_select(uint64_t arg0, uint64_t arg1,
	uint64_t arg2, uint64_t arg3,
	uint64_t arg4) {
    return forward_syscall(__NR_select, arg0, arg1, arg2, arg3, arg4); 
}

static int
hio_setsockopt(uint64_t arg0, uint64_t arg1,
	uint64_t arg2, uint64_t arg3,
	uint64_t arg4) {
    return forward_syscall(__NR_setsockopt, arg0, arg1, arg2, arg3, arg4); 
}

static int
hio_getsockopt(uint64_t arg0, uint64_t arg1,
	uint64_t arg2, uint64_t arg3,
	uint64_t arg4) {
    return forward_syscall(__NR_getsockopt, arg0, arg1, arg2, arg3, arg4); 
}

static int
forward_syscall(
	int    syscall_nr,
	uint64_t    arg0,
	uint64_t    arg1,
	uint64_t    arg2,
	uint64_t    arg3,
	uint64_t    arg4
) {
    printk(KERN_INFO "Forward syscall\n");

    /* Assume Linux and Kitten has compatible spinlocks*/
    spin_lock(&engine->lock);
    if ((engine->rb_syscall_prod_idx + 1) % HIO_RB_SIZE == engine->rb_ret_cons_idx) {
        spin_unlock(&engine->lock);
        printk(KERN_ERR "Error: engine ringbuffer is full\n");
        return -1;
    }
       
    struct hio_cmd_t *cmd = &engine->rb[engine->rb_syscall_prod_idx];
    cmd->stub_id = STUB_ID;
    cmd->syscall_nr = syscall_nr;
    cmd->arg0 = arg0;
    cmd->arg1 = arg1;
    cmd->arg2 = arg2;
    cmd->arg3 = arg3;
    cmd->arg4 = arg4;
    engine->rb_syscall_prod_idx = (engine->rb_syscall_prod_idx + 1) % HIO_RB_SIZE;

    spin_unlock(&engine->lock);

    struct pending_syscalls *pending = &pending_syscall_array[cmd->stub_id];
    wait_event_interruptible(pending->waitq, pending->is_pending);

    return pending->ret_val;
}

/* TODO: add a kthread to dispatch syscall returns */

static int
hio_module_init(void) {
    int ret = 0;
    int i;

    printk(KERN_INFO "Load Hobbes IO (HIO) module\n");

    /* TODO: this should be instialized by attach IOCTL */
    engine = (struct hio_engine *)kmalloc(
            sizeof(struct hio_engine), GFP_KERNEL);
    if (engine == NULL) {
        printk(KERN_ERR "Error attaching to xpmem hio_engine\n");
        return -1;
    }

    for (i = 0; i < MAX_STUBS; i++) {
        waitq_init(&pending_syscall_array[i].waitq);
    }

    kfs_create("/dev/hio",
        NULL,
        &hio_fops,
        0777,
        NULL,
        0);

    /* Attach to HIO Engine shared over xpmem */

    syscall_register( __NR_socket, (syscall_ptr_t) hio_socket );
    syscall_register( __NR_bind, (syscall_ptr_t) hio_bind );
    syscall_register( __NR_connect, (syscall_ptr_t) hio_connect );
    syscall_register( __NR_accept, (syscall_ptr_t) hio_accept );
    syscall_register( __NR_listen, (syscall_ptr_t) hio_listen );
    syscall_register( __NR_select, (syscall_ptr_t) hio_select );
    syscall_register( __NR_setsockopt, (syscall_ptr_t) hio_setsockopt );
    syscall_register( __NR_getsockopt, (syscall_ptr_t) hio_getsockopt );

    //syscall_register( __NR_sendto, (syscall_ptr_t) forward_syscall );
    //syscall_register( __NR_sendmsg, (syscall_ptr_t) forward_syscall );
    //syscall_register( __NR_recvfrom, (syscall_ptr_t) forward_syscall );
    //syscall_register( __NR_recvmsg, (syscall_ptr_t) forward_syscall );

    return ret;
}


DRIVER_INIT("module", hio_module_init);
//DRIVER_EXIT(hio_exit);
