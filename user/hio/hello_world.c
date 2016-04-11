/* Copyright (c) 2008, Sandia National Laboratories */
/* Copyright (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu> */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sched.h>
#include <pthread.h>
#include <lwk/liblwk.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <utmpx.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <math.h>
#include <lwk/rcr/rcr.h>
#include <sys/mman.h>

#define TEST_BLOCK_LAYER 1
//#define TEST_TASK_MEAS 1

static int pmem_api_test(void);
static int aspace_api_test(void);
static int task_api_test(void);
static int task_migrate_test(void);
static int fd_test(void);
static int socket_api_test(void);
static int hypervisor_api_test(void);
#ifdef TEST_BLOCK_LAYER
static int block_layer_test(void);
#endif
#ifdef TEST_TASK_MEAS
static int task_meas_api_test(void);
#endif
static int rcr_test(void);

int
main(int argc, char *argv[], char *envp[])
{
	int i;

	printf("Hello, world!\n");

        socket_api_test();

	printf("\n");
	printf("ALL TESTS COMPLETE\n");

	printf("\n");
	printf("Spinning forever...\n");
	while(1)
		sleep(100000);
}

int
socket_api_test( void )
{
        printf("\nTEST BEGIN: Sockets API\n");

        int port = 80;
        int s = socket( PF_INET, SOCK_STREAM, 0 );
        printf( "ERROR: socket() returns %d\n", s );
        return 0;

        if( s < 0 )
        {
                printf( "ERROR: socket() failed! rc=%d\n", s );
                return -1;
        }

        struct sockaddr_in addr, client;
        addr.sin_family = AF_INET;
        addr.sin_port = htons( port );
        addr.sin_addr.s_addr = INADDR_ANY;
        if( bind( s, (struct sockaddr*) &addr, sizeof(addr) ) < 0 )
                perror( "bind" );

        if( listen( s, 5 ) < 0 ) {
                perror( "listen" );
                return -1;
        }

        int max_fd = s;
        fd_set fds;
        FD_ZERO( &fds );
        FD_SET( s, &fds );

        printf( "Going into mainloop: server fd %d on port %d\n", s, port );

        while(1)
        {
                fd_set read_fds = fds;
                int rc = select( max_fd+1, &read_fds, 0, 0, 0 );
                if( rc < 0 )
                {
                        printf( "ERROR: select() failed! rc=%d\n", rc );
                        return -1;
                }

                if( FD_ISSET( s, &read_fds ) )
                {
                        printf( "new connection\n" );
                        socklen_t socklen = sizeof(client);
                        int new_fd = accept(
                                s,
                                (struct sockaddr *) &client,
                                &socklen
                        );

                        printf( "connected newfd %d!\n", new_fd );
                        writen( new_fd, "Welcome!\n", 9 );
                        FD_SET( new_fd, &fds );
                        if( new_fd > max_fd )
                                max_fd = new_fd;
                }

                int fd;
                for( fd=0 ; fd<=max_fd ; fd++ )
                {
                        if( fd == s || !FD_ISSET( fd, &read_fds ) )
                                continue;

                        char buf[ 33 ];
                        ssize_t rc = read( fd, buf, sizeof(buf)-1 );
                        if( rc <= 0 )
                        {
                                printf( "%d: closed connection rc=%zd\n", fd, rc );
                                FD_CLR( fd, &fds );
                                continue;
                        }

                        if( rc == 0 )
                                continue;

                        buf[rc] = '\0';

                        // Trim any newlines off the end
                        while( rc > 0 )
                        {
                                if( !isspace( buf[rc-1] ) )
                                        break;
                                buf[--rc] = '\0';
                        }

                        printf( "%d: read %zd bytes: '%s'\n",
                                fd,
                                rc,
                                buf
                        );

                        char outbuf[ 32 ];
                        int len = snprintf( outbuf, sizeof(outbuf), "%d: read %zd bytes\n", fd, rc );
                        writen( fd, outbuf, len );
                }
        }

        printf("TEST END: Sockets API\n");
        return 0;
}
