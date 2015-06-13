/* dc.c */

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dc {
	int *stack;
	int top;	/* last filled slot */
	int stacksz;
	int iradix;
	int oradix;
	int debug;
};

void dc_init(struct dc *dc) {
	dc->iradix = 0;
	dc->oradix = 10;
	dc->debug = 0;
	dc->stacksz = 1024;
	dc->top = -1;
	dc->stack = calloc(dc->stacksz, sizeof *dc->stack);
	if (!dc->stack)
		err(2, "calloc(%d,%lu)", dc->stacksz, sizeof *dc->stack);
}

void dc_push(struct dc *dc, int v) {
	if (dc->top >= dc->stacksz - 1)
		errx(1, "dstack overflow");
	dc->stack[++dc->top] = v;
}

int dc_pop(struct dc *dc) {
	if (dc->top < 0)
		errx(1, "dstack underflow");
	return dc->stack[dc->top--];
}

int dc_peek(struct dc *dc) {
	if (dc->top < 0)
		errx(1, "dstack underflow");
	return dc->stack[dc->top];
}

void dc_pushn(struct dc *dc, char *num) {
	char *ep = NULL;
	int v = strtol(num, &ep, dc->iradix);
	if (*ep != '\0')
		errx(1, "invalid number '%s'", num);
	dc_push(dc, v);
}

void dcf_binop(struct dc *dc, char *cmd) {
	int v0 = dc_pop(dc);
	int v1 = dc_pop(dc);
	int v2 = 0;
	switch (*cmd) {
		case '+': v2 = v0 + v1; break;
		case '-': v2 = v0 - v1; break;
		case '*': v2 = v0 * v1; break;
		case '/': v2 = v0 / v1; break;
		case '%': v2 = v0 % v1; break;
		case '^': v2 = v0 ^ v1; break;
		case '&': v2 = v0 & v1; break;
		case '|': v2 = v0 | v1; break;
		default:
			errx(2, "dcf_binop(%s)", cmd);
			break;
	}
	dc_push(dc, v2);
}

void dcf_print(struct dc *dc, char *cmd) {
	int v = *cmd == 'n' ? dc_pop(dc) : dc_peek(dc);
	char *fmts = NULL;
	switch (dc->oradix) {
		case 10: fmts = "%d\n"; break;
		case 16: fmts = "%x\n"; break;
		case 8: fmts = "%o\n"; break;
		default:
			errx(2, "unknown oradix %d", dc->oradix);
			break;
	}
	printf(fmts, v);
}

void dcf_printall(struct dc *dc, char *cmd) {
	char *fmts = NULL;
	int i;
	switch (dc->oradix) {
		case 10: fmts = " %d"; break;
		case 16: fmts = " %x"; break;
		case 8: fmts = " %o"; break;
		default:
			errx(2, "unknown oradix %d", dc->oradix);
			break;
	}
	printf("stack:");
	for (i = 0; i <= dc->top; i++) {
		printf(fmts, dc->stack[i]);
	}
	printf("$\n");
}

void dcf_cfgset(struct dc *dc, char *cmd) {
	int v = dc_pop(dc);
	switch (*cmd) {
		case 'i': dc->iradix = v; break;
		case 'o': dc->oradix = v; break;
		case 'b': dc->debug = v; break;
		default:
			errx(2, "unknown cfgset %s", cmd);
			break;
	}
}

void dcf_cfgget(struct dc *dc, char *cmd) {
	int v = 0;
	switch (*cmd) {
		case 'I': v = dc->iradix; break;
		case 'O': v = dc->oradix; break;
		case 'b': v = dc->debug; break;
		default:
			errx(2, "unknown cfgget %s", cmd);
			break;
	}
	dc_push(dc, v);
}

void dcf_clear(struct dc *dc, char *cmd) {
	dc->top = -1;
}

void dcf_dup(struct dc *dc, char *cmd) {
	int v = dc_peek(dc);
	dc_push(dc, v);
}

void dcf_swap(struct dc *dc, char *cmd) {
	int v0 = dc_pop(dc);
	int v1 = dc_pop(dc);
	dc_push(dc, v0);
	dc_push(dc, v1);
}

void dcf_unop(struct dc *dc, char *cmd) {
	int v = dc_pop(dc);
	switch (*cmd) {
		case '!': v = !v; break;
		case '~': v = ~v; break;
		default:
			errx(2, "unknown unop '%s'", cmd);
			break;
	}
	dc_push(dc, v);
}

struct {
	const char *name;
	void (*func)(struct dc *dc, char *cmd);
} dc_cmds[] = {
	{ "+", dcf_binop },
	{ "-", dcf_binop },
	{ "*", dcf_binop },
	{ "/", dcf_binop },
	{ "%", dcf_binop },
	{ "^", dcf_binop },
	{ "&", dcf_binop },
	{ "|", dcf_binop },
	{ "I", dcf_cfgget },
	{ "O", dcf_cfgget },
	{ "B", dcf_cfgget },
	{ "i", dcf_cfgset },
	{ "o", dcf_cfgset },
	{ "b", dcf_cfgset },
	{ "c", dcf_clear },
	{ "d", dcf_dup },
	{ "p", dcf_print },
	{ "n", dcf_print },
	{ "f", dcf_printall },
	{ "r", dcf_swap },
	{ "!", dcf_unop },
	{ "~", dcf_unop },
};

void dc_exec(struct dc *dc, char *cmd) {
	int i;
	if (isdigit(*cmd) || *cmd == '_') {
		dc_pushn(dc, cmd);
		return;
	}
	for (i = 0; dc_cmds[i].name; i++) {
		if (strcmp(cmd, dc_cmds[i].name))
			continue;
		if (dc->debug) {
			printf("before %s ", cmd);
			dcf_printall(dc, "f");
		}
		dc_cmds[i].func(dc, cmd);
		if (dc->debug) {
			printf("after %s ", cmd);
			dcf_printall(dc, "f");
		}
		return;
	}
	errx(1, "unknown command '%s'", cmd);
}

int main(int argc, char *argv[]) {
	char *s;
	struct dc dc;

	if (argc < 2)
		errx(1, "Usage: %s <commands>", argv[0]);

	dc_init(&dc);
	s = strtok(argv[1], " ");
	while (s) {
		dc_exec(&dc, s);
		s = strtok(NULL, " ");
	}
}
