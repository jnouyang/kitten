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

// hardcoded STUB_ID currently support on process only
#define STUB_ID 3

static int forward_syscall(int syscall_nr, uint64_t arg0, uint64_t arg1, 
        uint64_t arg2, uint64_t arg3, uint64_t arg4); 
static int insert_syscall(int syscall_nr, uint64_t arg0, uint64_t arg1, 
        uint64_t arg2, uint64_t arg3, uint64_t arg4); 

static struct hio_engine *engine = NULL;

struct pending_ret {
    int stub_id;
    spinlock_t lock;

    waitq_t waitq;

    int ret_val;
    int is_pending;
};

static struct pending_ret pending_ret_array[MAX_STUBS];

static int
hio_open(struct inode *inode, struct file *file) {
    return 0;
}


static int
hio_release(struct inode * inodep, struct file  * filp) {
    return 0;
}

/*
static void dump_page(void *buf) {
    unsigned long long *ptr = buf;
    int i, j;
    int col_num = 8;

    printk(KERN_INFO "magic %p\n", &engine->magic);
    printk(KERN_INFO "prod_idx %p\n", &engine->rb_syscall_prod_idx);
    printk(KERN_INFO "cons_idx %p\n", &engine->rb_syscall_cons_idx);
    printk(KERN_INFO "ret prod_idx %p\n", &engine->rb_ret_prod_idx);
    printk(KERN_INFO "ret cons_idx %p\n", &engine->rb_ret_cons_idx);

    // 4KB page is 512 * 8bytes
    for (i = 0; i < 512/col_num; i++) {
        for (j = 0; j < col_num; j++) {
            printk(KERN_INFO "%llx ", *ptr);
            ptr++;
        }
        printk(KERN_INFO "\n");
    }
}
*/


static void engine_dispachter_loop(void) {
    printk(KERN_INFO "Enter HIO engine dispather loop...\n");

    if (engine == NULL) {
        printk(KERN_ERR "engine is NULL\n");
    }

    //dump_page((void *)engine);
    //printk(KERN_INFO "Test fording getpid syscall\n");
    //insert_syscall(39, 0, 0, 0, 0, 0);

    do {
        while (engine->rb_ret_prod_idx != engine->rb_ret_cons_idx) {
            pisces_spin_lock(&engine->lock);

            if (engine->rb_ret_prod_idx == engine->rb_ret_cons_idx)
                break;

            struct hio_cmd_t *cmd = &(engine->rb[engine->rb_ret_cons_idx]);

            printk(KERN_INFO "ENGINE dispatcher: consume ret index %d (prod index %d)\n", 
                    engine->rb_ret_cons_idx,
                    engine->rb_ret_prod_idx);

            struct pending_ret *ret = &pending_ret_array[cmd->stub_id];

            if (ret->is_pending) {
                printk(KERN_ERR "Another ret is pending for stub_id %d???\n", cmd->stub_id);
                goto out;
            } else {
                spin_lock(&ret->lock);
                ret->ret_val = cmd->ret_val;
                printk(KERN_INFO "Return value is %d\n", ret->ret_val);
                ret->is_pending = true;
                spin_unlock(&ret->lock);
            }

out:
            engine->rb_ret_cons_idx = (engine->rb_ret_cons_idx + 1) % HIO_RB_SIZE;
            pisces_spin_unlock(&engine->lock);

            wake_up_interruptible(&ret->waitq);
        } 

        schedule();
    } while(1);
}

/*
 * User ioctl to the HIO driver. 
 */
static long
hio_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    long ret;

    switch (cmd) {
        /* Get xemem address from userspace 
         * Enter the dispatcher loop
         */
        case HIO_IOCTL_ENGINE_ATTACH: 
            {
                void *buf = (void __user *)arg;
                paddr_t   addr_pa     = 0;
                int *ptr;

                printk("HIO engine attach...\n");

                if (aspace_virt_to_phys(current->aspace->id, (vaddr_t) buf, &addr_pa) < 0) {
                    printk(KERN_ERR "Invalid user address for hio engine attach: %p\n", buf);
                    return -1;
                }

                ptr = __va(addr_pa);
                printk("    buffer uva %p, pa %p, kva %p, content %x\n", buf, (void *)addr_pa, ptr, *ptr);

                engine = (struct hio_engine *)ptr;

                engine_dispachter_loop();

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
insert_syscall(
	int    syscall_nr,
	uint64_t    arg0,
	uint64_t    arg1,
	uint64_t    arg2,
	uint64_t    arg3,
	uint64_t    arg4
) {
    pisces_spin_lock(&engine->lock);
    if ((engine->rb_syscall_prod_idx + 1) % HIO_RB_SIZE == engine->rb_ret_cons_idx) {
        pisces_spin_unlock(&engine->lock);
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

    pisces_spin_unlock(&engine->lock);
    return 0;
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
    printk(KERN_INFO "Forward syscall %d\n", syscall_nr);

    if (insert_syscall(syscall_nr, arg0, arg1, arg2, arg3, arg4) < 0)
        return -1;

    struct pending_ret *ret = &(pending_ret_array[STUB_ID]);
    wait_event_interruptible(ret->waitq, ret->is_pending);

    return ret->ret_val;
}

/*
 * Create /dev/hio
 * Register syscall handlers
 */
static int
hio_module_init(void) {
    int ret = 0;
    int i;

    printk(KERN_INFO "Load Hobbes IO (HIO) module\n");

    /* TODO: this should be instialized by attach IOCTL */
    /*
    engine = (struct hio_engine *)kmalloc(
            sizeof(struct hio_engine), GFP_KERNEL);
    if (engine == NULL) {
        printk(KERN_ERR "Error attaching to xpmem hio_engine\n");
        return -1;
    }
    */

    for (i = 0; i < MAX_STUBS; i++) {
        pending_ret_array[i].is_pending = false;
        waitq_init(&pending_ret_array[i].waitq);
        spin_lock_init(&pending_ret_array[i].lock);
    }

    kfs_create("/dev/hio",
        NULL,
        &hio_fops,
        0777,
        NULL,
        0);

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
