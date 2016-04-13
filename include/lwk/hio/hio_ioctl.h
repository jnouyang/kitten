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


// hio commands
#define HIO_IOCTL_ENGINE_START      3300
#define HIO_IOCTL_REGISTER          3301 // register stub
#define HIO_IOCTL_DEREGISTER        3302

#define HIO_IOCTL_ENGINE_ATTACH     3303

#endif
