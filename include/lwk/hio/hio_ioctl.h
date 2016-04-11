/*
 * HobbesIO system call forwarding service
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#ifndef _HIO_IOCTL_H_
#define _HIO_IOCTL_H_

#include <lwk/types.h>
#include <arch/ioctl.h>

struct hio_cmd_attach {
	char name[32]; /* xemem segment name */
};

/*
 * ioctl() commands
 */
#define HIO_MAGIC               33	
#define HIO_CMD_ATTACH	        _IOW(HIO_MAGIC, 0, struct hio_cmd_attach)

#endif
