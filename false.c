/* false.c - compiler for http://en.wikipedia.org/wiki/FALSE
 * ops: + - * / _ = > & |
 * $ dup, % pop, \ swap, @ rot, O pick, [ ... ] fn, fn ! call, bool fn ? if
 * fn pred # while, . putd, , putc, ^ getc, B fflush, { } comment
 *
 * Sample programs:
 * "Hello, World!"
 * 99 [ $ 0 = ~ ] [ $ . " bottles of beer on the wall!" 1 - ] #
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define push(p)		"\tsubq $0x8, %%r15\n\tmovq " p ", (%%r15)\n"
#define pop(p)		"\tmovq (%%r15), " p "\n\taddq $0x8, %%r15\n"
#define peek(p)		"\tmovq (%%r15), " p "\n"

struct buf {
	struct buf *next;
	int id;
	size_t sz;
	char *bytes;
};

static struct buf *bufs = NULL;

char *mklabel() {
	static int id = 0;
	static char buf[32];
	snprintf(buf, sizeof(buf), "F_%d", id++);
	return buf;
}

struct buf *mkbuf() {
	static int id = 0;
	struct buf *b = malloc(sizeof *b);
	b->sz = 0;
	b->bytes = NULL;
	b->id = id++;
	b->next = bufs;
	bufs = b;
	return b;
}

void bputs(struct buf *b, char *s) {
	b->bytes = realloc(b->bytes, b->sz + strlen(s) + 1);
	memcpy(b->bytes + b->sz, s, strlen(s) + 1);
	b->sz += strlen(s);
}

struct buf *compilestr(char *in, char **out) {
	struct buf *b = mkbuf();
	char *e = strchrnul(in, '"');
	*out = *e ? e + 1 : NULL;
	*e = '\0';
	bputs(b, "\t.asciz \"");
	bputs(b, in);
	bputs(b, "\"\n");
	return b;
}

struct buf *compile(char *in, char **out) {
	struct buf *b = mkbuf();
	char fmt[256];
	char fclab0[32], fclab1[32];
	bputs(b, "\tpushq %r8\n");
	bputs(b, "\tpushq %r9\n");
	while (*in) {
		if (isspace(*in)) {
			in++;
			continue;
		} else if (*in == '\'') {
			in++;
			snprintf(fmt, sizeof(fmt),
			         push("$0x%02x"), *in);
			in++;
			continue;
		} else if (isdigit(*in)) {
			uint64_t l = strtoull(in, &in, 0);
			snprintf(fmt, sizeof(fmt),
			         push("$0x%016llx"),
			         l);
			bputs(b, fmt);
			continue;
		} else if (*in == '"') {
			struct buf *c = compilestr(in + 1, &in);
			snprintf(fmt, sizeof(fmt),
			         "\tleaq L_%d, %%rdi\n\tcall puts\n",
			         c->id);
			bputs(b, fmt);
			continue;
		} else if (*in == '[') {
			struct buf *c = compile(in + 1, &in);
			snprintf(fmt, sizeof(fmt),
			         "\tleaq L_%d, %%rax\n"
			         push("%%rax"),
			         c->id);
			bputs(b, fmt);
			continue;
		} else if (*in == ']') {
			break;
		} else if (*in == '{') {
			while (*in && *in != '}')
				in++;
			if (*in)
				in++;
			continue;
		} else if (*in >= 'a' && *in <= 'z') {
			char v = *in++ - 'a';
			if (*in == ':') {
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax")
				         "\tmov %%rax, %d(%%r14)\n",
				         v * 8);
			} else if (*in == ';') {
				snprintf(fmt, sizeof(fmt),
				         "\tmov %d(%%r14), %%rax\n"
				         push("%%rax"),
				         v * 8);
			} else {
				break;
			}
			in++;
			bputs(b, fmt);
			continue;
		}

		switch (*in) {
			case '_':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax")
				         "\timulq $-1, %%rax\n"
				         push("%%rax"));
				break;
			case '+':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax")
				         pop("%%rdx")
				         "\taddq %%rdx, %%rax\n"
				         push("%%rax"));
				break;
			case '-':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax")
				         pop("%%rdx")
				         "\tsubq %%rax, %%rdx\n"
				         push("%%rdx"));
				break;
			case '*':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax")
				         pop("%%rdx")
				         "\timulq %%rdx, %%rax\n"
				         push("%%rax"));
				break;
			case '/':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax")
				         pop("%%rdx")
				         "\tdivq %%rdx, %%rax\n"
				         push("%%rax"));
				break;
			case '=':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax") pop("%%rdx")
				         "\tmovq $1, %%rcx\n"
				         "\tcmp %%rax, %%rdx\n"
				         "\tmovq $0, %%rax\n"
				         "\tcmove %%rcx, %%rax\n"
				         push("%%rax"));
				break;
			case '>':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax") pop("%%rdx")
				         "\tmovq $1, %%rcx\n"
				         "\tcmp %%rax, %%rdx\n"
				         "\tmovq $0, %%rax\n"
				         "\tcmovg %%rcx, %%rax\n"
				         push("%%rax"));
				break;
			case '&':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax") pop("%%rdx")
				         "\tmov $0, %%rdi\n"
				         "\tmov $1, %%rcx\n"
				         "\ttest %%rax,%%rax\n"
					 "\tcmovz %%rdi, %%rcx\n"
				         "\ttest %%rdx,%%rdx\n"
				         "\tcmovz %%rdi, %%rcx\n"
				         push("%%rcx"));
				break;
			case '|':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax") pop("%%rdx")
				         "\tmov $1, %%rdi\n"
				         "\tmov $0, %%rcx\n"
				         "\ttest %%rax,%%rax\n"
					 "\tcmovnz %%rdi, %%rcx\n"
				         "\ttest %%rdx,%%rdx\n"
				         "\tcmovnz %%rdi, %%rcx\n"
				         push("%%rcx"));
				break;
			case '~':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax")
				         "\tmovq $1, %%rcx\n"
				         "\ttest %%rax,%%rax\n"
				         "\tmovq $0, %%rax\n"
				         "\tcmovz %%rcx, %%rax\n"
				         push("%%rax"));
				break;
			case '$':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax")
				         push("%%rax")
				         push("%%rax"));
				break;
			case '%':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rcx"));
				break;
			case '\\':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax") pop("%%rdx")
				         push("%%rax") push("%%rdx"));
				break;
			case '@':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax") pop("%%rdx") pop("%%rcx")
				         push("%%rdx") push("%%rax")
				         push("%%rcx"));
				break;
			case 'O':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax")
					 "\tmov (%%r15,%%rax,8), %%rax\n"
				         push("%%rax"));
				break;
			case '.':
				snprintf(fmt, sizeof(fmt),
				         "\txorq %%rax, %%rax\n"
				         pop("%%rsi")
				         "\tleaq _fmt_int, %%rdi\n"
				         "\tcallq printf\n");
				break;
			case ',':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rdi")
				         "\tcallq putchar\n");
				break;
			case '^':
				snprintf(fmt, sizeof(fmt),
				         "\tcallq getchar\n"
				         "\tmovsxd %%eax, %%rdx\n"
				         push("%%rdx"));
				break;
			case 'B':
				snprintf(fmt, sizeof(fmt),
				         "\tmovq stdin, %%rdi\n"
				         "\tcallq fflush\n"
				         "\tmovq stdout, %%rdi\n"
				         "\tcallq fflush\n");
				break;
			case '!':
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax")
				         "\tcallq *%%rax\n");
				break;
			case '?':
				strcpy(fclab0, mklabel());
				snprintf(fmt, sizeof(fmt),
				         pop("%%rax")
				         pop("%%rdx")
				         "\ttestq %%rdx, %%rdx\n"
				         "\tjz %s\n"
				         "\tcallq *%%rax\n"
				         "%s:\n",
				         fclab0, fclab0);
				break;
			case '#':
				strcpy(fclab0, mklabel());
				strcpy(fclab1, mklabel());
				snprintf(fmt, sizeof(fmt),
				         pop("%%r8")
				         pop("%%r9")
				         "%s:\n"
				         "\tcallq *%%r9\n"
				         pop("%%rax")
				         "\ttestq %%rax,%%rax\n"
				         "\tjz %s\n"
				         "\tcallq *%%r8\n"
				         "\tjmp %s\n"
				         "%s:\n",
				         fclab1, fclab0, fclab1, fclab0);
				break;
			default:
				break;
		}
		bputs(b, fmt);
		in++;
	}
	bputs(b, "\tpopq %r9\n");
	bputs(b, "\tpopq %r8\n");
	snprintf(fmt, sizeof(fmt), "\tret\n");
	bputs(b, fmt);
	*out = *in ? in + 1 : NULL;
	return b;
}

int main(int argc, char *argv[]) {
	char *_dummy;
	struct buf *in = mkbuf();
	struct buf *b;
	struct buf *q;
	char lb[4096];

	bufs = NULL;

	memset(lb, 0, sizeof(lb));
	while (fgets(lb, sizeof(lb) - 1, stdin)) {
		bputs(in, lb);
		memset(lb, 0, sizeof(lb));
	}

	b = compile(in->bytes, &_dummy);

	printf(".data\n.space 0x10000\nstack:\nvars:\n.space 0xd0\n.text\n");
	printf("_fmt_int: .asciz \"%%d\\n\"\n");
	printf(".global main\n");
	for (q = bufs; q; q = q->next) {
		if (q->id > 1) {
			printf("L_%d:\n", q->id);
		} else {
			printf("main:\n");
			printf("\tleaq stack, %%r15\n");
			printf("\tleaq vars, %%r14\n");
		}
		printf("%s", q->bytes);
	}

	fflush(stdout);
	return 0;
}
