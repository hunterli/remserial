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
 */

#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

#define CFLG	0
#define IFLG	1
#define OFLG	2
#define LFLG	3
#define RFLG	4
#define BFLG	5

extern int errno;

static struct sttyset {
	char *name;
	int which;
	int mask;
	int value;
} sttynames[] = {
	{ "0",		BFLG,	0,		B0	},
	{ "50",		BFLG,	0,		B50	},
	{ "75",		BFLG,	0,		B75	},
	{ "110",	BFLG,	0,		B110	},
	{ "134",	BFLG,	0,		B134	},
	{ "150",	BFLG,	0,		B150	},
	{ "200",	BFLG,	0,		B200	},
	{ "300",	BFLG,	0,		B300	},
	{ "600",	BFLG,	0,		B600	},
	{ "1200",	BFLG,	0,		B1200	},
	{ "1800",	BFLG,	0,		B1800	},
	{ "2400",	BFLG,	0,		B2400	},
	{ "4800",	BFLG,	0,		B4800	},
	{ "9600",	BFLG,	0,		B9600	},
	{ "19200",	BFLG,	0,		B19200	},
	{ "38400",	BFLG,	0,		B38400	},
#ifdef B57600
	{ "57600",	BFLG,	0,		B57600	},
#endif
#ifdef B115200
	{ "115200",	BFLG,	0,		B115200	},
#endif
#ifdef B230400
	{ "230400",	BFLG,	0,		B230400	},
#endif
	{ "cs7",	CFLG,	CSIZE,		CS7	},
	{ "cs8",	CFLG,	CSIZE,		CS8	},
	{ "cstopb",	CFLG,	CSTOPB,		CSTOPB	},
	{ "cread",	CFLG,	CREAD,		CREAD	},
	{ "parenb",	CFLG,	PARENB,		PARENB	},
	{ "parodd",	CFLG,	PARODD,		PARODD	},
	{ "hubcl",	CFLG,	HUPCL,		HUPCL	},
	{ "clocal",	CFLG,	CLOCAL,		CLOCAL	},
#ifdef CRTSCTS
	{ "crtscts",	CFLG,	CRTSCTS,	CRTSCTS	},
#endif
#ifdef ORTSFL
	{ "ortsfl",	CFLG,	ORTSFL,		ORTSFL	},
#endif
#ifdef CTSFLOW
	{ "ctsflow",	CFLG,	CTSFLOW,	CTSFLOW	},
#endif
#ifdef RTSFLOW
	{ "rtsflow",	CFLG,	RTSFLOW,	RTSFLOW	},
#endif
	{ "ignbrk",	IFLG,	IGNBRK,		IGNBRK	},
	{ "brkint",	IFLG,	BRKINT,		BRKINT	},
	{ "ignpar",	IFLG,	IGNPAR,		IGNPAR	},
	{ "parmrk",	IFLG,	PARMRK,		PARMRK	},
	{ "inpck",	IFLG,	INPCK,		INPCK	},
	{ "istrip",	IFLG,	ISTRIP,		ISTRIP	},
	{ "inlcr",	IFLG,	INLCR,		INLCR	},
	{ "igncr",	IFLG,	IGNCR,		IGNCR	},
	{ "icrnl",	IFLG,	ICRNL,		ICRNL	},
#ifdef IUCLC	// Missing on OSX, FreeBSD
	{ "iuclc",	IFLG,	IUCLC,		IUCLC	},
#endif
	{ "ixon",	IFLG,	IXON,		IXON	},
	{ "ixany",	IFLG,	IXANY,		IXANY	},
	{ "ixoff",	IFLG,	IXOFF,		IXOFF	},
#ifdef IMAXBEL
	{ "imaxbel",	IFLG,	IMAXBEL,	IMAXBEL	},
#endif
	{ "opost",	OFLG,	OPOST,		OPOST	},
#ifdef ILCUC	// Missing on OSX, FreeBSD
	{ "olcuc",	OFLG,	OLCUC,		OLCUC	},
#endif
	{ "onlcr",	OFLG,	ONLCR,		ONLCR	},
	{ "ocrnl",	OFLG,	OCRNL,		OCRNL	},
	{ "onocr",	OFLG,	ONOCR,		ONOCR	},
	{ "onlret",	OFLG,	ONLRET,		ONLRET	},
	{ "ofil",	OFLG,	OFILL,		OFILL	},
	{ "ofdel",	OFLG,	OFDEL,		OFDEL	},
	{ "nl0",	OFLG,	NLDLY,		NL0	},
	{ "nl1",	OFLG,	NLDLY,		NL1	},
	{ "cr0",	OFLG,	CRDLY,		CR0	},
	{ "cr1",	OFLG,	CRDLY,		CR1	},
	{ "cr2",	OFLG,	CRDLY,		CR2	},
	{ "cr3",	OFLG,	CRDLY,		CR3	},
	{ "tab0",	OFLG,	TABDLY,		TAB0	},
	{ "tab1",	OFLG,	TABDLY,		TAB1	},
	{ "tab2",	OFLG,	TABDLY,		TAB2	},
	{ "tab3",	OFLG,	TABDLY,		TAB3	},
	{ "bs0",	OFLG,	BSDLY,		BS0	},
	{ "bs1",	OFLG,	BSDLY,		BS1	},
	{ "vt0",	OFLG,	VTDLY,		VT0	},
	{ "vt1",	OFLG,	VTDLY,		VT1	},
	{ "ff0",	OFLG,	FFDLY,		FF0	},
	{ "ff1",	OFLG,	FFDLY,		FF1	},
	{ "isig",	LFLG,	ISIG,		ISIG	},
	{ "icanon",	LFLG,	ICANON,		ICANON	},
#ifdef XCASE	// Missing on OSX, FreeBSD
	{ "xcase",	LFLG,	XCASE,		XCASE	},
#endif
	{ "echo",	LFLG,	ECHO,		ECHO	},
	{ "echoe",	LFLG,	ECHOE,		ECHOE	},
	{ "echok",	LFLG,	ECHOK,		ECHOK	},
	{ "echonl",	LFLG,	ECHONL,		ECHONL	},
	{ "noflsh",	LFLG,	NOFLSH,		NOFLSH	},
	{ "tostop",	LFLG,	TOSTOP,		TOSTOP	},
#ifdef ECHOCTL
	{ "echoctl",	LFLG,	ECHOCTL,	ECHOCTL	},
#endif
#ifdef ECHOPRT
	{ "echoprt",	LFLG,	ECHOPRT,	ECHOPRT	},
#endif
#ifdef ECHOKE
	{ "echoke",	LFLG,	ECHOKE,		ECHOKE	},
#endif
#ifdef FLUSHO
	{ "flusho",	LFLG,	FLUSHO,		FLUSHO	},
#endif
#ifdef PENDIN
	{ "pendin",	LFLG,	PENDIN,		PENDIN	},
#endif
	{ "iexten",	LFLG,	IEXTEN,		IEXTEN	},
#ifdef TOSTOP
	{ "tostop",	LFLG,	TOSTOP,		TOSTOP	},
#endif
	{ "raw",	RFLG,	0,		0	},
	{ NULL,		0,	0,		0	} };

static void
set_this_tty(struct termios *term,struct sttyset *p,int turnon)
{
	/*
	pdebug(5,"set_this_tty: setting %s on? %d\n",p->name,turnon);
	*/
	switch ( p->which ) {
	case CFLG:
		term->c_cflag &= ~(p->mask);
		if ( turnon )
			term->c_cflag |= p->value;
		break;
	case IFLG:
		term->c_iflag &= ~(p->mask);
		if ( turnon )
			term->c_iflag |= p->value;
		break;
	case OFLG:
		term->c_oflag &= ~(p->mask);
		if ( turnon )
			term->c_oflag |= p->value;
		break;
	case LFLG:
		term->c_lflag &= ~(p->mask);
		if ( turnon )
			term->c_lflag |= p->value;
		break;
	case RFLG:
		term->c_iflag = 0;
		term->c_oflag = 0;
		term->c_lflag = 0;
		term->c_cc[VMIN] = 1;
		term->c_cc[VTIME] = 0;
		break;
	case BFLG:
		cfsetispeed(term, p->value);
		cfsetospeed(term, p->value);
		break;
	}
}

int
set_tty(int fd,char *settings)
{
	register char *p;
	register char *s;
	struct termios term;
	register int i;
	int mode;

	/*
	pdebug(4,"set_tty: fd %d settings %s\n",fd,settings);
	*/
	if ( tcgetattr(fd,&term) == -1 ) {
		/*
		pdebug(4,"set_tty: cannot get settings for fd %d, error %d\n",
			fd,errno);
		*/
		return -1;
	}

	s = strdup(settings);
	p = strtok(s," \t\n");
	while (p) {
		mode = 1;
		if ( *p == '-' ) {
			mode = 0;
			p++;
		}
		for ( i=0 ; sttynames[i].name ; i++ ) {
			if ( !strcmp(p,sttynames[i].name) ) {
				set_this_tty(&term,&sttynames[i],mode);
				break;
			}
		}
		p = strtok(NULL," \t\n");
	}
	free(s);
	if ( tcsetattr(fd,TCSANOW,&term) == -1 ) {
		/*
		pdebug(4,"set_tty: cannot get settings for fd %d error %d\n",
			fd,errno);
		*/
		return -1;
	}
	else
		return 0;
}
