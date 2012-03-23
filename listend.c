/* listend.c - terminates line-buffered TCP sockets to stdio
 * requires -lelib */

#include <elib/linesocket.h>
#include <elib/map.h>
#include <elib/reactor.h>
#include <elib/util.h>
#include <elib/uuid.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXLINE 512

static struct map *clients;

struct client {
	struct linesocket *ls;
	char uuid[UUIDSTRSIZE];
};

static void udie(const char *prefix) {
	perror(prefix);
	exit(1);
}

static void client_done(struct linesocket *ls) {
	close(ls->s->fd);
}

static void client_line(struct linesocket *ls, char *line) {
	struct client *c = ls->priv;
	char buf[MAXLINE * 2];
	snprintf(buf, sizeof(buf), "read %s %s\n", c->uuid, line);
	write(1, buf, strlen(buf));
}

static void client_close(struct linesocket *ls) {
	struct client *c = ls->priv;
	char buf[MAXLINE * 2];
	snprintf(buf, sizeof(buf), "close %s\n", c->uuid);
	write(1, buf, strlen(buf));
	map_put(clients, c->uuid, NULL);
	reactor_del(ls->s->r, ls->s);
	efree(c, sizeof *c);
}

static struct client *client_new(struct socket *s) {
	struct linesocket *l = linesocket_new(s, MAXLINE);
	struct client *c = emalloc(sizeof *c);
	unsigned char uuid[UUIDLEN];
	if (!clients)
		clients = map_new();
	uuidgen(uuid);
	l->line = client_line;
	l->close = client_close;
	l->priv = c;
	c->ls = l;
	uuid2str(c->uuid, uuid);
	map_put(clients, c->uuid, c);
	return c;
}

static void listener_read(struct socket *s) {
	struct sockaddr_in sa;
        socklen_t salen = sizeof(sa);
        int nfd = accept(s->fd, (struct sockaddr *)&sa, &salen);
        struct socket *n;
	struct client *c;
        if (nfd == -1)
                udie("accept()");
        n = reactor_add(s->r, nfd);
	c = client_new(n);
	char buf[MAXLINE * 2];
	snprintf(buf, sizeof(buf), "new %s\n", c->uuid);
	write(1, buf, strlen(buf));
}

static int serve(const char *hstr, const char *pstr) {
	int port = atoi(pstr);
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in sa;
	int reuse = 1;
	if (sfd == -1)
		udie("socket()");
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(port);
	if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)))
		udie("setsockopt()");
	if (bind(sfd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
		udie("bind()");
	if (listen(sfd, 20) < 0)
		udie("listen()");
	return sfd;
}

static void stdio_line(struct linesocket *ls, char *line) {
	char *cmd, *who, *arg0;
	struct client *c;
	char mbuf[MAXLINE];
	cmd = strtok(line, " ");
	if (!cmd)
		return;
	who = strtok(NULL, " ");
	if (!who)
		return;
	if (!clients)
		return;
	c = map_get(clients, who);
	if (!c)
		return;
	if (!strcmp(cmd, "write")) {
		arg0 = strtok(NULL, "");
		estrlcpy(mbuf, arg0, sizeof(mbuf) - 2);
		estrlcat(mbuf, "\r\n", sizeof(mbuf));
		linesocket_write(c->ls, mbuf);
	} else if (!strcmp(cmd, "kill")) {
		arg0 = strtok(NULL, "");
		estrlcpy(mbuf, arg0, sizeof(mbuf) - 2);
		estrlcat(mbuf, "\r\n", sizeof(mbuf));
		c->ls->writedone = client_done;
		linesocket_write(c->ls, mbuf);
	}
}

int main(int argc, char *argv[]) {
	struct reactor *r;
	struct socket *listener;
	struct socket *stdio;
	struct linesocket *lstdio;

	emalloc_fatal = 1;

	if (argc < 3) {
		printf("Usage: %s address port\n", argv[0]);
		return 1;
	}

	r = reactor_new();
	listener = reactor_add(r, serve(argv[1], argv[2]));
	listener->read = listener_read;
	reactor_refresh(r, listener);
	stdio = reactor_add(r, 0);
	lstdio = linesocket_new(stdio, 1024);
	lstdio->line = stdio_line;

	while (1)
		reactor_run(r);
	return 0;
}
