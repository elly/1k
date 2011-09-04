 /* See LICENSE file for license details. */
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *host = NULL;
static char *port = NULL;
static int bufsrvidx = 0;
static char bufsrv[4096];
static int bufcliidx = 0;
static char bufcli[4096];
static char bufout[4096];
static char bufout2[4096];
static int srv;

#include "util.c"

static void pout(char *channel, char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

static void sout(char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(bufout, sizeof bufout, fmt, ap);
	va_end(ap);
	snprintf(bufout2, sizeof bufout2, "%s\r\n", bufout);
	write(srv, bufout2, strlen(bufout2));
}

static void parsein(char *s) {
	skip(s, '\n');
	skip(s, '\r');
	sout("%s", s);
}

static void parsesrv(char *cmd) {
	skip(cmd, '\r');
	if(!strncmp("PING ", cmd, 5)) {
		sout("PONG %s", cmd + 5);
	}
	pout("%s\n", cmd);
}

static int readc(int fd) {
	char c;
	if (read(fd, &c, 1) <= 0) { return -1; }
	return c;
}

static int readb(int fd, char *buf, int *idx) {
	int c = readc(fd);
	int wasnl;

	if (c < 0) { return -1; }
	wasnl = c == '\n';
	buf[*idx] = c == '\n' ? '\0' : c;
	(*idx)++;
	return wasnl;
}

int main(int argc, char *argv[]) {
	int i;
	fd_set rd;
	fd_set ed;

	if (argc < 3) {
		printf("sic: Usage: %s <host> <port>\n", argv[0]);
		return 0;
	}

	host = argv[1];
	port = argv[2];

	setbuf(stdout, NULL);

	/* init */
	srv = dial(host, port);
	for(;;) { /* main loop */
		FD_ZERO(&rd);
		FD_SET(0, &rd);
		FD_SET(srv, &rd);
		FD_ZERO(&ed);
		FD_SET(0, &ed);
		FD_SET(srv, &ed);
		i = select(srv + 1, &rd, 0, &ed, NULL);
		if (i < 0) {
			if(errno == EINTR)
				continue;
			eprint("sic: error on select():");
		}
		if (FD_ISSET(srv, &ed)) {
			eprint("sic: dead socket:");
			close(srv);
			return 1;
		}
		if (FD_ISSET(0, &ed)) {
			eprint("sic: lost tty:");
			close(srv);
			return 2;
		}
		if (FD_ISSET(srv, &rd)) {
			i = readb(srv, bufsrv, &bufsrvidx);
			if (i < 0) {
				eprint("sic: dead socket:");
				close(srv);
				return 1;
			}
			if (i) {
				parsesrv(bufsrv);
				bufsrvidx = 0;
			}
		}
		if(FD_ISSET(0, &rd)) {
			i = readb(0, bufcli, &bufcliidx);
			if (i < 0) {
				eprint("sic: lost tty:");
				close(srv);
				return 2;
			}
			if (i) {
				parsein(bufcli);
				bufcliidx = 0;
			}
		}
	}
	return 0;
}
