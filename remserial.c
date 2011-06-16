/*
 * remserial
 * Copyright (C) 2000  Paul Davis, pdavis@lpccomp.bc.ca
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * This program acts as a bridge either between a socket(2) and a
 * serial/parallel port or between a socket and a pseudo-tty.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

struct sockaddr_in addr,remoteaddr;
int sockfd = -1;
int port = 23000;
int debug=0;
int devfd;
int *remotefd;
char *machinename = NULL;
char *sttyparms = NULL;
static char *sdevname = NULL;
char *linkname = NULL;
int isdaemon = 0;
fd_set fdsread,fdsreaduse;
struct hostent *remotehost;
extern char* ptsname(int fd);
int curConnects = 0;

void sighandler(int sig);
int connect_to(struct sockaddr_in *addr);
void usage(char *progname);
void link_slave(int fd);

int main(int argc, char *argv[])
{
	int result;
	extern char *optarg;
	extern int optind;
	int maxfd = -1;
	char devbuf[512];
	int devbytes;
	int remoteaddrlen;
	int c;
	int waitlogged = 0;
	int maxConnects = 1;
	int writeonly = 0;
	register int i;

	while ( (c=getopt(argc,argv,"dl:m:p:r:s:wx:")) != EOF )
		switch (c) {
		case 'd':
			isdaemon = 1;
			break;
		case 'l':
			linkname = optarg;
			break;
		case 'x':
			debug = atoi(optarg);
			break;
		case 'm':
			maxConnects = atoi(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'r':
			machinename = optarg;
			break;
		case 's':
			sttyparms = optarg;
			break;
		case 'w':
			writeonly = 1;
			break;
		case '?':
			usage(argv[0]);
			exit(1);
		}

	sdevname = argv[optind];
	remotefd = (int *) malloc (maxConnects * sizeof(int));

	// struct group *getgrgid(gid_t gid);

	/*
	printf("sdevname=%s,port=%d,stty=%s\n",sdevname,port,sttyparms);
	*/

	openlog("remserial", LOG_PID, LOG_USER);

	if (writeonly)
		devfd = open(sdevname,O_WRONLY);
	else
		devfd = open(sdevname,O_RDWR);
	if ( devfd == -1 ) {
		syslog(LOG_ERR, "Open of %s failed: %m",sdevname);
		exit(2);
	}

	if (linkname)
		link_slave(devfd);

	if ( sttyparms ) {
		set_tty(devfd,sttyparms);
	}

	signal(SIGINT,sighandler);
	signal(SIGHUP,sighandler);
	signal(SIGTERM,sighandler);

	if ( machinename ) {
		/* We are the client,
		   Find the IP address for the remote machine */
		remotehost = gethostbyname(machinename);
		if ( !remotehost ) {
			syslog(LOG_ERR, "Couldn't determine address of %s",
				machinename );
			exit(3);
		}

		/* Copy it into the addr structure */
		addr.sin_family = AF_INET;
		memcpy(&(addr.sin_addr),remotehost->h_addr_list[0],
			sizeof(struct in_addr));
		addr.sin_port = htons(port);

		remotefd[curConnects++] = connect_to(&addr);
	}
	else {
		/* We are the server */

		/* Open the initial socket for communications */
		sockfd = socket(AF_INET, SOCK_STREAM, 6);
		if ( sockfd == -1 ) {
			syslog(LOG_ERR, "Can't open socket: %m");
			exit(4);
		}

		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = 0;
		addr.sin_port = htons(port);
	 
		/* Set up to listen on the given port */
		if( bind( sockfd, (struct sockaddr*)(&addr),
			sizeof(struct sockaddr_in)) < 0 ) {
			syslog(LOG_ERR, "Couldn't bind port %d, aborting: %m",port );
			exit(5);
		}
		if ( debug>1 )
			syslog(LOG_NOTICE,"Bound port");

		/* Tell the system we want to listen on this socket */
		result = listen(sockfd, 4);
		if ( result == -1 ) {
			syslog(LOG_ERR, "Socket listen failed: %m");
			exit(6);
		}

		if ( debug>1 )
			syslog(LOG_NOTICE,"Done listen");
	}

	if ( isdaemon ) {
		setsid();
		close(0);
		close(1);
		close(2);
	}

	/* Set up the files/sockets for the select() call */
	if ( sockfd != -1 ) {
		FD_SET(sockfd,&fdsread);
		if ( sockfd >= maxfd )
			maxfd = sockfd + 1;
	}

	for (i=0 ; i<curConnects ; i++) {
		FD_SET(remotefd[i],&fdsread);
		if ( remotefd[i] >= maxfd )
			maxfd = remotefd[i] + 1;
	}

	if (!writeonly) {
		FD_SET(devfd,&fdsread);
		if ( devfd >= maxfd )
			maxfd = devfd + 1;
	}

	while (1) {

		/* Wait for data from the listening socket, the device
		   or the remote connection */
		fdsreaduse = fdsread;
		if ( select(maxfd,&fdsreaduse,NULL,NULL,NULL) == -1 )
			break;

		/* Activity on the controlling socket, only on server */
		if ( !machinename && FD_ISSET(sockfd,&fdsreaduse) ) {
			int fd;

			/* Accept the remote systems attachment */
			remoteaddrlen = sizeof(struct sockaddr_in);
			fd = accept(sockfd,(struct sockaddr*)(&remoteaddr),
				&remoteaddrlen);

			if ( fd == -1 )
				syslog(LOG_ERR,"accept failed: %m");
			else if (curConnects < maxConnects) {
				unsigned long ip;

				remotefd[curConnects++] = fd;
				/* Tell select to watch this new socket */
				FD_SET(fd,&fdsread);
				if ( fd >= maxfd )
					maxfd = fd + 1;
				ip = ntohl(remoteaddr.sin_addr.s_addr);
				syslog(LOG_NOTICE, "Connection from %d.%d.%d.%d",
					(int)(ip>>24)&0xff,
					(int)(ip>>16)&0xff,
					(int)(ip>>8)&0xff,
					(int)(ip>>0)&0xff);
			}
			else {
				// Too many connections, just close it to reject
				close(fd);
			}
		}

		/* Data to read from the device */
		if ( FD_ISSET(devfd,&fdsreaduse) ) {
			devbytes = read(devfd,devbuf,512);
			//if ( debug>1 && devbytes>0 )
			if (debug>1)
				syslog(LOG_INFO,"Device: %d bytes",devbytes);
			if ( devbytes <= 0 ) {
				if ( debug>0 )
					syslog(LOG_INFO,"%s closed",sdevname);
				close(devfd);
				FD_CLR(devfd,&fdsread);
				while (1) {
					devfd = open(sdevname,O_RDWR);
					if ( devfd != -1 )
						break;
					syslog(LOG_ERR, "Open of %s failed: %m",
						sdevname);
					if ( errno != EIO )
						exit(7);
					sleep(1);
				}
				if ( debug>0 )
					syslog(LOG_INFO,"%s re-opened",sdevname);
				if ( sttyparms )
					set_tty(devfd,sttyparms);
				if (linkname)
					link_slave(devfd);
				FD_SET(devfd,&fdsread);
				if ( devfd >= maxfd )
					maxfd = devfd + 1;
			}
			else
				for (i=0 ; i<curConnects ; i++)
					write(remotefd[i],devbuf,devbytes);
		}

		/* Data to read from the remote system */
		for (i=0 ; i<curConnects ; i++)
			if (FD_ISSET(remotefd[i],&fdsreaduse) ) {

				devbytes = read(remotefd[i],devbuf,512);

				//if ( debug>1 && devbytes>0 )
				if (debug>1)
					syslog(LOG_INFO,"Remote: %d bytes",devbytes);

				if ( devbytes == 0 ) {
					register int j;

					syslog(LOG_NOTICE,"Connection closed");
					close(remotefd[i]);
					FD_CLR(remotefd[i],&fdsread);
					curConnects--;
					for (j=i ; j<curConnects ; j++)
						remotefd[j] = remotefd[j+1];
					if ( machinename ) {
						/* Wait for the server again */
						remotefd[curConnects++] = connect_to(&addr);
						FD_SET(remotefd[curConnects-1],&fdsread);
						if ( remotefd[curConnects-1] >= maxfd )
							maxfd = remotefd[curConnects-1] + 1;
					}
				}
				else if ( devfd != -1 )
					/* Write the data to the device */
					write(devfd,devbuf,devbytes);
		}
	}
	close(sockfd);
	for (i=0 ; i<curConnects ; i++)
		close(remotefd[i]);
}

void sighandler(int sig)
{
	int i;

	if ( sockfd != -1 )
		close(sockfd);
	for (i=0 ; i<curConnects ; i++)
		close(remotefd[i]);
	if ( devfd != -1 )
		close(devfd);
	if (linkname)
		unlink(linkname);
	syslog(LOG_ERR,"Terminating on signal %d",sig);
	exit(0);
}

void link_slave(int fd)
{
	char *slavename;
	int status = grantpt(devfd);
	if (status != -1)
		status = unlockpt(devfd);
	if (status != -1) {
		slavename = ptsname(devfd);
		if (slavename) {
			// Safety first
			unlink(linkname);
			status = symlink(slavename, linkname);
		}
		else
			status = -1;
	}
	if (status == -1) {
		syslog(LOG_ERR, "Cannot create link for pseudo-tty: %m");
		exit(8);
	}
}

int
connect_to(struct sockaddr_in *addr)
{
	int waitlogged = 0;
	int stat;
	extern int errno;
	int sockfd;

	if ( debug>0 ) {
		unsigned long ip = ntohl(addr->sin_addr.s_addr);
		syslog(LOG_NOTICE, "Trying to connect to %d.%d.%d.%d",
			(int)(ip>>24)&0xff,
			(int)(ip>>16)&0xff,
			(int)(ip>>8)&0xff,
			(int)(ip>>0)&0xff);
	}

	while (1) {
		/* Open the socket for communications */
		sockfd = socket(AF_INET, SOCK_STREAM, 6);
		if ( sockfd == -1 ) {
			syslog(LOG_ERR, "Can't open socket: %m");
			exit(9);
		}

		/* Try to connect to the remote server,
		   if it fails, keep trying */

		stat = connect(sockfd, (struct sockaddr*)addr,
			sizeof(struct sockaddr_in));
		if ( debug>1 )
			if (stat == -1)
				syslog(LOG_NOTICE, "Connect status %d, errno %d: %m\n", stat,errno);
			else
				syslog(LOG_NOTICE, "Connect status %d\n", stat);

		if ( stat == 0 )
			break;
		/* Write a message to syslog once */
		if ( ! waitlogged ) {
			syslog(LOG_NOTICE,
				"Waiting for server on %s port %d: %m",
				machinename,port );
			waitlogged = 1;
		}
		close(sockfd);
		sleep(10);
	}
	if ( waitlogged || debug>0 )
		syslog(LOG_NOTICE,
			"Connected to server %s port %d",
			machinename,port );
	return sockfd;
}

void usage(char *progname) {
	printf("Remserial version 1.3.  Usage:\n");
	printf("remserial [-r machinename] [-p netport] [-s \"stty params\"] [-m maxconnect] device\n\n");

	printf("-r machinename		The remote machine name to connect to.  If not\n");
	printf("			specified, then this is the server side.\n");
	printf("-p netport		Specifiy IP port# (default 23000)\n");
	printf("-s \"stty params\"	If serial port, specify stty parameters, see man stty\n");
	printf("-m max-connections	Maximum number of simultaneous client connections to allow\n");
	printf("-d			Run as a daemon program\n");
	printf("-x debuglevel		Set debug level, 0 is default, 1,2 give more info\n");
	printf("-l linkname		If the device name is a pseudo-tty, create a link to the slave\n");
	printf("-w          		Only write to the device, no reading\n");
	printf("device			I/O device, either serial port or pseudo-tty master\n");
}
