/*
 * HobbesIO system call forwarding service
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#ifndef _HIO_IOCTL_H_
#define _HIO_IOCTL_H_

#include <lwk/types.h>
#include <arch/ioctl.h>

/*
 * ioctl() commands
 */
#define HIO_TYPE                33	
#define HIO_CMD_TEST	        _IO(HIO_TYPE, 0)

struct hio_cmd_connect {
	char name[32]; /* xemem segment name */
};

#endif
