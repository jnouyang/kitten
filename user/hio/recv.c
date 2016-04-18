/*
 * recv -- a single-thread blocking data recving server
 * (c) Jiannan Ouyang <ouyang@cs.pitt.edu>, 03/12/2016
 *
 * Blocking on multiple incoming connections using select,
 * recving data, then reply done.
 *
 * Data formet: struct incoming_data {int data_len, binary raw_data};
 *
 * This protocal is compatible with a modified version of mutilate to 
 * measure tail latencies
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/syscall.h>
#include <arch/unistd.h>

#define PORTNUM 3369   // port we're listening on
struct sockaddr_in serverAddr;

static int hio_select(int nfds, fd_set *readfds, fd_set *writefds, 
			fd_set *exceptfds, struct timeval *timeout) {
    return syscall(__NR_HIO_select, nfds, readfds, writefds, exceptfds, timeout);
    //return syscall(__NR_select, nfds, readfds, writefds, exceptfds, timeout);
}
static int hio_close(int fd) {
    return syscall(__NR_HIO_close, fd);
    //return syscall(__NR_close, fd);
}

static int create_and_bind (void) {
	int fd;
	int yes=1;        // for setsockopt() SO_REUSEADDR, below

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons( PORTNUM );

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("error: socket");
		return -1;
	}

	// lose the pesky "address already in use" error message
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	if (bind(fd,
		(const struct sockaddr *)&serverAddr,
		sizeof(serverAddr)) < 0) {
		printf("error: bind");
		return -1;
	}

	return fd;
}

int main(void)
{
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int s;
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int i;
    int len;
    char *buf = NULL;
#define BUF_SIZE 4096*10
    buf =(char *)malloc(BUF_SIZE);
    int buflen = BUF_SIZE;
    if (buf == NULL) {
	    perror("init malloc");
	    abort();
    }

    listener = create_and_bind();
    if (listener == -1) abort();

    // listen
    s = listen(listener, SOMAXCONN);
    if (s == -1) {
	    perror ("listen");
	    abort ();
    }
    printf("Listening at socket %d port %d\n", listener, PORTNUM);

    FD_ZERO(&master);
    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (hio_select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            return -1;
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
		    newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

		    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
			printf("[INFO] new connection from %s on " "socket %d\n",
				inet_ntop(remoteaddr.ss_family,
					&(((struct sockaddr_in*)&remoteaddr)->sin_addr),
					remoteIP,
					INET6_ADDRSTRLEN),
				newfd);
                    }
                } else {
#define HEADER_SIZE 4
			// handle data from a client
			nbytes = recv(i, buf, HEADER_SIZE, 0);
			if (nbytes < HEADER_SIZE) {
				if (nbytes < 0 && errno != ECONNRESET) {
                                    printf("[ERROR] recv header ret %d, errno is not supported by HIO\n", i);
                                    goto out;
                                }
				printf("[INFO] socket hung up in recv header: %d\n", i);
				hio_close(i); // bye!
				FD_CLR(i, &master); // remove from master set
				break;
			}

			len = ntohl(*(int *)buf);
			//printf("len %d\n", len);

			// dynamic heap allocation
			if (len > buflen) {
				if (buf != NULL) free(buf);
				buf = (char *)malloc(len*2);
				buflen = len * 2;
                                printf("resizing buffer to %d\n", buflen);
				if (buf == NULL) {
					perror("malloc");
					abort();
				}
			}

			int left = len;
			nbytes = 0;
			while (left > 0) {
				//printf("%d bytes left\n", left);
				nbytes = recv(i, buf, left, 0);
				if (nbytes <= 0) {
					if (nbytes < 0 && errno != ECONNRESET) {
                                            printf("[ERROR] recv data ret %d, errno is not supported by HIO\n", i);
                                            goto out;
                                        }
					printf("[INFO] socket hung up in recv data: %d\n", i);
					//printf("%d: received %d bytes, nbytes %d\n", i, len+HEADER_SIZE, nbytes);
					hio_close(i); // bye!
					FD_CLR(i, &master); // remove from master set
					break;
				}
				left = len - nbytes;
				//printf("len %d, nbytes %d, %d bytes left\n", len, nbytes, left);
			}
			if (nbytes <= 0) {
				//printf("break %d\n", i);
				break;
			}

			//printf("%d: received %d bytes, nbytes %d\n", i, len+HEADER_SIZE, nbytes);
			nbytes = send(i, "Done", 5, 0);
			if (nbytes <= 0) {
				if (nbytes < 0) {
                                    printf("[ERROR] send ret %d, errno is not supported by HIO\n", i);
                                    goto out;
                                }
				printf("[INFO] socket hung up in send: %d\n", i);
				hio_close(i); // bye!
				FD_CLR(i, &master); // remove from master set
			}
			//printf("write 5 bytes\n");
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!

out:
    printf("Tearing down Mutilate clint may cause errno 104 (ECONNRESET) to server read/write,\n");
    printf("But the performance numbers measured are still valid.\n");

    return 0;
}

