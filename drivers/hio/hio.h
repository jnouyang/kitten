/* 
 * HIO Header
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#ifndef _HIO_H_
#define _HIO_H_

#include <lwk/hio/hio_ioctl.h>
#include <lwk/types.h>
#include <linux/wait.h>
#include <linux/cdev.h>

#define MAX_STUBS               32
#define HIO_RB_SIZE             MAX_STUBS

// transferred in the ringbuffer
struct __attribute__((__packed__)) hio_cmd_t {
    int stub_id;
    int syscall_nr;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    int ret_val;
    int errno;
};


struct __attribute__((__packed__)) hio_stub {
    int stub_id;
    struct hio_engine *hio_engine;

    spinlock_t                  lock;
    struct stub_syscall_t      *pending_syscall; 
    bool                        is_pending;
    wait_queue_head_t           syscall_wq;

    dev_t           dev; 
    struct cdev     cdev;
};


struct __attribute__((__packed__)) hio_engine {
    uint32_t    magic;
    int                         rb_syscall_prod_idx;     // shared, updated by hio client
    int                         rb_ret_cons_idx;         // client private
    int                         rb_syscall_cons_idx;     // engine private
    int                         rb_ret_prod_idx;         // shared, updated by hio engine
    struct hio_cmd_t            rb[HIO_RB_SIZE];
    spinlock_t                  lock;

    // We could use hashmap here, but for now just use array
    // and use rank number as the key
    //struct hio_stub *stub_lookup_table[MAX_STUBS];

    //struct task_struct         *handler_thread;

    // wait for syscall requests
    //wait_queue_head_t           syscall_wq;
};

#endif
