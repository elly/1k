/* zed.c - line-oriented editor ala ed(1) */

#include <err.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	MAXFN = 1024,
	MAXLINE = 4096,
};

struct line {
	char *data;
	size_t sz;
};

struct buffer {
	char fn[MAXFN];
	struct line *l;
	size_t nl;
	size_t a1, a2;
	size_t dot;
};

/* grow b, inserting space for n lines after after */
static void grow(struct buffer *b, size_t after, size_t n) {
	b->l = realloc(b->l, (b->nl + n) * sizeof(*b->l));
	if (!b->l)
		err(1, "realloc");
	memmove(b->l + after + n, b->l + after, sizeof(*b->l) * (b->nl - after));
	b->nl += n;
}

/* shrink b, deleting the n lines after after */
static void shrink(struct buffer *b, size_t after, size_t n) {
	struct line *nl = calloc(b->nl - n, sizeof(*nl));
	if (!nl)
		err(1, "calloc");
	memmove(nl, b->l, after * sizeof(*nl));
	memmove(nl + after, b->l + after + n, (b->nl - after - n) * sizeof(*nl));
	b->l = nl;
	b->nl -= n;
	if (b->dot > b->nl)
		b->dot = b->nl;
}

static void empty(struct buffer *b) {
	shrink(b, 0, b->nl);
}

static void mdup(struct line *d, const struct line *s) {
	void *nd = malloc(s->sz);
	if (!nd)
		err(1, "malloc");
	memcpy(nd, s->data, s->sz);
	d->data = nd;
	d->sz = s->sz;
}

static void mmov(struct line *d, struct line *s) {
	d->data = s->data;
	d->sz = s->sz;
}

static size_t nbytes(struct buffer *b) {
	size_t i;
	size_t n = 0;
	for (i = 0; i < b->nl; i++)
		n += b->l[i].sz - 1;	/* skip null term */
	return n;
}

void addline(struct buffer *b, struct line *l, size_t at) {
	grow(b, at, 1);
	mdup(b->l + at, l);
}

void addbuf(struct buffer *d, struct buffer *s, size_t after) {
	size_t i;
	grow(d, after, s->nl);
	for (i = 0; i < s->nl; i++)
		mmov(d->l + after + i, s->l + i);
	shrink(s, s->nl, 0);
}

void rdbuf(struct buffer *b, FILE* f) {
	char lb[MAXLINE];
	struct line ll;
	while (fgets(lb, sizeof(lb), f)) {
		if (!strcmp(lb, ".\n"))
			break;
		if (strchr(lb, '\n'))
			*strchr(lb, '\n') = '\0';
		ll.data = lb;
		ll.sz = strlen(lb) + 1;
		addline(b, &ll, b->nl);
	}
}

void prbuf(struct buffer *b) {
	printf("%zu %zu\n", b->nl, nbytes(b));
}

void clamp(struct buffer *b) {
	if (b->dot > b->nl || !b->dot)
		b->dot = 1;
	if (b->a2 > b->nl)
		b->a2 = b->nl;
	if (b->a1 > b->a2)
		b->a1 = b->a2;
}

static jmp_buf cmdjmp;

void oops(const char *msg) {
	warn("%s", msg);
	longjmp(cmdjmp, 1);
}

void oopsx(const char *msg) {
	warnx("%s", msg);
	longjmp(cmdjmp, 1);
}

FILE *fropen(const char *fn) {
	FILE *f = fopen(fn, "r");
	if (!f)
		oops("fopen");
	return f;
}

FILE *fwopen(const char *fn) {
	FILE *f = fopen(fn, "w");
	if (!f)
		oops("fopen");
	return f;
}

int hasarg(const char *buf) {
	return buf[1] == ' ' && buf[1] != '\0';
}

void needarg(const char *buf) {
	if (!hasarg(buf))
		oopsx("needarg");
}

void neednonzero(struct buffer *fb) {
	if (fb->a1 == 0 || fb->a2 == 0)
		oopsx("neednonzero");
}

void edit(struct buffer *fb, char *fn) {
	FILE *f;
	if (strlen(fn) > sizeof(fb->fn))
		oopsx("longfname");
	f = fropen(fn);
	memcpy(fb->fn, fn, strlen(fn));
	empty(fb);
	rdbuf(fb, f);
	prbuf(fb);
	fb->dot = fb->nl;
}

void rdaddbuf(struct buffer *fb, size_t at, FILE *f) {
	struct buffer bb;
	memset(&bb, 0, sizeof(bb));
	rdbuf(&bb, f);
	addbuf(fb, &bb, at);
}

static int ndigits(size_t n) {
	int i = 0;
	while (n) {
		i++;
		n /= 10;
	}
	return i;
}

static int wrlnr(size_t stopat, size_t n) {
	int nd = ndigits(stopat);
	printf("%0*zu ", nd, n);
}

void wrbuf(struct buffer *fb, size_t at, size_t stopat, int number) {
	while (at <= stopat) {
		if (number)
			wrlnr(stopat, at);
		printf("%s\n", fb->l[at++ - 1].data);
	}
}

void svbuf(struct buffer *fb, const char *fn) {
	FILE *f;
	size_t n;
	if (!*fn)
		oopsx("needarg");
	f = fwopen(fn);
	for (n = 0; n < fb->nl; n++)
		if (fprintf(f, "%s\n", fb->l[n].data) < 0)
			oops("fprintf");
	if (fclose(f))
		oops("fclose");
}

void delbuf(struct buffer *fb, size_t a1, size_t a2) {
	shrink(fb, a1 - 1, a2 - a1 + 1);
}

char *getre(char **lb) {
	char *lb0 = *lb;
	char *re;
	if (*lb0 != '/')
		return NULL;
	lb0++;
	re = lb0;
	while (*lb0 && *lb0 != '/')
		lb0++;
	if (*lb0 != '/')
		oopsx("badre");
	*lb0 = '\0';
	lb0++;
	*lb = lb0;
	return re;
}

int findre1(struct line *l, char *re, size_t *cpos) {
	size_t i;
	size_t relen = strlen(re);
	if ((l->sz - 1) < relen)
		return 1;
	for (i = 0; i <= l->sz - relen - 1; i++) {
		if (!strncmp(l->data + i, re, relen)) {
			if (cpos)
				*cpos = i;
			return 0;
		}
	}
	return 1;
}

int findre(struct buffer *b, char *re, size_t *ln, size_t *cpos) {
	size_t end = b->dot < b->nl ? b->dot : 0;
	size_t i = end;
	do {
		if (!findre1(b->l + i, re, cpos)) {
			*ln = i;
			return 0;
		}
		i++;
		if (i >= b->nl)
			i = 0;
	} while (i != end);
	return 1;
}

char *getoneaddr(struct buffer *b, size_t *addr, char *lb) {
	size_t v = 0;
	char *lb0 = lb;
	int sign = 0;
	int nbase = 0;

	if (*lb0 == '.') {
		*addr = b->dot;
		nbase++;
		lb0++;
	}

	if (*lb0 == '$') {
		*addr = b->nl;
		nbase++;
		lb0++;
	}

	if (*lb0 == '/') {
		size_t cpos = 0;
		char *re = getre(&lb0);
		if (!findre(b, re, addr, &cpos)) {
			nbase++;
			(*addr)++;
		} else {
			oopsx("nomatch");
		}
	}

	if (*lb0 == '+') {
		sign = 1;
		lb0++;
	} else if (*lb0 == '-') {
		sign = -1;
		lb0++;
	}

	while (isdigit(*lb0)) {
		v = v * 10 + *lb0 - '0';
		lb0++;
	}

	if (!sign && !nbase)
		*addr = v;
	else
		*addr += v * sign;

	if (*lb0 == ',')
		lb0++;

	return lb0;
}

int getaddr(struct buffer *fb, char *lb) {
	char *lb0, *lb1;
	fb->a1 = fb->dot;
	lb0 = getoneaddr(fb, &fb->a1, lb);
	fb->a2 = fb->a1;
	lb1 = getoneaddr(fb, &fb->a2, lb0);
	if (lb0 == lb1)
		fb->a2 = fb->a1;
	if (fb->a2 < fb->a1) {
		size_t t = fb->a2;
		fb->a2 = fb->a1;
		fb->a1 = t;
	}
	return lb1 - lb;
}

void subst1(struct line *l, char *re, char *p) {
	size_t cpos;
	size_t n = l->sz + strlen(p) - strlen(re);
	char *data;
	if (findre1(l, re, &cpos))
		return;
	data = malloc(n);
	if (!data)
		err(1, "malloc");
	memcpy(data, l->data, cpos);
	memcpy(data + cpos, p, strlen(p));
	memcpy(data + cpos + strlen(p), l->data + cpos + strlen(re), l->sz - cpos - strlen(re));
	free(l->data);
	l->data = data;
	l->sz = n;
}

void subst(struct buffer *fb, char *lb) {
	char *re = getre(&lb);
	size_t i;
	size_t j;
	char *p;

	if (!re)
		oopsx("needre");
	p = lb;
	while (*lb && *lb != '/')
		lb++;
	if (*lb != '/')
		oopsx("needsubst");
	*lb = '\0';

	for (i = fb->a1; i <= fb->a2; i++)
		subst1(fb->l + i - 1, re, p);
}

void cmd(struct buffer *fb, char *lb);

void gre(struct buffer *fb, char *lb) {
	char *re = getre(&lb);
	size_t i;
	char cb[MAXLINE];
	if (!re)
		oopsx("needre");
	if (!*lb)
		oopsx("needcmd");
	for (i = 0; i < fb->nl; i++) {
		if (findre1(fb->l + i, re, NULL))
			continue;
		snprintf(cb, sizeof(cb), "%zu%s", i + 1, lb);
		cmd(fb, cb);
	}
}

void cmd(struct buffer *fb, char *lb) {
	int i = getaddr(fb, lb);
	int number = 0;
	size_t oldnl = fb->nl;
	clamp(fb);

	switch (lb[i]) {
		case 'a':
			rdaddbuf(fb, fb->nl ? fb->dot : 0, stdin);
			fb->dot = fb->nl;
			break;
		case 'd':
			delbuf(fb, fb->a1, fb->a2);
			break;
		case 'e':
			needarg(lb + i);
			edit(fb, lb + i + 2);
			break;
		case 'g':
			gre(fb, lb + i + 1);
			break;
		case 'i':
			rdaddbuf(fb, fb->a1, stdin);
			fb->dot = fb->a1 + (fb->nl - oldnl);
			break;
		case 'n':
			number = 1;
		case 'p':
			neednonzero(fb);
			wrbuf(fb, fb->a1, fb->a2, number);
			fb->dot = fb->a2;
			break;
		case 'q':
			exit(0);
			break;
		case 's':
			neednonzero(fb);
			subst(fb, lb + i + 1);
			break;
		case 'w':
			svbuf(fb, hasarg(lb + i) ? lb + i + 2 : fb->fn);
			prbuf(fb);
			if (lb[i + 1] == 'q')
				exit(0);
			break;
		case '?':
			printf("(.)a          append\n");
			printf("(.,.)d        delete\n");
			printf("e <fn>        edit fn\n");
			printf("g/re/c        run c on lines matching re\n");
			printf("(.)i          insert\n");
			printf("(.,.)n        print numbered\n");
			printf("(.,.)p        print\n");
			printf("(.,.)s/re/v   replace re with v\n");
			printf("q             quit\n");
			printf("w <fn>        write fn\n");
			break;
		default:
			oopsx("nocmd, try '?'");
			break;
	}
}

void cmds(struct buffer *fb, char *ifn) {
	char lb[MAXLINE];
	memset(lb, 0, sizeof(lb));

	if (ifn && !setjmp(cmdjmp))
		edit(fb, ifn);

	while (1) {
		setjmp(cmdjmp);
		printf("zed$ ");
		fflush(stdout);
		if (!fgets(lb, sizeof(lb), stdin))
			break;
		if (strchr(lb, '\n'))
			*strchr(lb, '\n') = '\0';
		cmd(fb, lb);
		memset(lb, 0, sizeof(lb));
	}
}

int main(int argc, char *argv[]) {
	struct buffer fb;
	int i;
	memset(&fb, 0, sizeof(fb));

	cmds(&fb, argc > 1 ? argv[1] : NULL);

	return 0;
}
