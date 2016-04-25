/*
 * HobbesIO system call forwarding service
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#ifndef _HIO_IOCTL_H_
#define _HIO_IOCTL_H_

#include <lwk/types.h>
#include <arch/ioctl.h>

#define HIO_ENGINE_MAGIC        0x13131313

#define HIO_ENGINE_PAGE_SIZE    4096
#define HIO_ENGINE_SEG_NAME     "hio_engine_seg"

// /dev/hio commands
#define HIO_IOCTL_ENGINE_START      3300
#define HIO_IOCTL_REGISTER          3301 // register stub
#define HIO_IOCTL_DEREGISTER        3302
#define HIO_IOCTL_ENGINE_ATTACH     3303
#define HIO_IOCTL_SYSCALL_RET       3304

// /dev/hio-stub commands
#define HIO_STUB_SYSCALL_POLL       3400
#define HIO_STUB_SYSCALL_RET        3401
#define HIO_STUB_TEST_SYSCALL       3402


// used in ioctls
struct hio_syscall_t {
    int stub_id;
    int syscall_nr;
    unsigned long long arg0;
    unsigned long long arg1;
    unsigned long long arg2;
    unsigned long long arg3;
    unsigned long long arg4;
};


// used in ioctls
struct hio_syscall_ret_t {
    int stub_id;
    int syscall_nr;
    int ret_val;
    int ret_errno;
};

#endif
