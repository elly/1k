/* xxc.c - hex calculator
 *
 * xxc is analogous to dc, but intended for manipulating binary data.
 * xxc's core data structure is a stack. Each element in the stack is one of:
 *   An integer (a machine word)
 *   A string of printable characters
 *   A blob of arbitrary bytes
 *   A vector of integers
 * All of xxc's operations manipulate this stack in some way.
 *
 * Integers are written as any of:
 *   0x[hex value]
 *   0o[octal value]
 *   0b[binary value]
 *   [decimal value]
 * Strings are written as:
 *   "string contents", with C-style escapes allowed
 * Blobs are written as:
 *   #[ hex digits ]
 * Vectors are written as:
 *   #{ integers }
 */

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* A blob might own its data or not. If it owns its data, its viewing member is
 * null and it will free its data when its refcount drops to zero; otherwise, it
 * will hold a ref to its viewing member. */
struct blob {
	int refcount;
	struct blob *viewing;
	unsigned char *data;
	size_t len;
};

void blob_ref(struct blob *b) {
	b->refcount++;
}

void blob_unref(struct blob *b) {
	b->refcount--;
	if (b->refcount != 0)
		return;
	if (b->viewing)
		blob_unref(b->viewing);
	else
		free(b->data);
}

struct blob *blob_new_data(unsigned char *data, size_t len) {
	struct blob *b = malloc(sizeof *b);
	b->refcount = 1;
	b->viewing = NULL;
	b->data = data;
	b->len = len;
	return b;
}

struct blob *blob_new_view(struct blob *viewing, size_t offset, size_t len) {
	struct blob *b = malloc(sizeof *b);
	b->refcount = 1;
	b->viewing = viewing;
	b->data = viewing->data + offset;
	b->len = len;
	return b;
}

struct vec {
	int64_t *data;
	size_t len;
	size_t filled;
};

struct vec *vec_new(size_t isize) {
	struct vec *v = malloc(sizeof *v);
	v->data = calloc(isize, sizeof(int64_t));
	v->len = isize;
	v->filled = 0;
	return v;
}

struct vec *vec_dup(struct vec *vv) {
	struct vec *v = vec_new(vv->len);
	memcpy(v->data, vv->data, vv->filled * sizeof(int64_t));
	v->filled = vv->filled;
	return v;
}

void vec_free(struct vec *v) {
	free(v->data);
	free(v);
}

void vec_grow(struct vec *v) {
	v->data = realloc(v->data, 2 * v->len * sizeof(*v->data));
	memset(v->data + v->len, 0, v->len * sizeof(*v->data));
	v->len *= 2;
}

void vec_add(struct vec *v, int64_t val) {
	if (v->filled == v->len)
		vec_grow(v);
	v->data[v->filled++] = val;
}

void vec_put(struct vec *v, size_t idx, int64_t val) {
	while (idx >= v->len)
		vec_grow(v);
	v->data[idx] = val;
}

enum valtype {
	VAL_INT,
	VAL_STR,
	VAL_BLOB,
	VAL_VEC,
};

struct val {
	enum valtype type;
	struct val *next;
	union {
		int64_t ival;
		char *sval;
		struct blob *bval;
		struct vec *vval;
	} u;
};

struct val *stack = NULL;

void push(struct val *val) {
	val->next = stack;
	stack = val;
}

struct val *pop() {
	struct val *v = stack;
	if (stack)
		stack = stack->next;
	return v;
}

struct val *peek() {
	return stack;
}

size_t depth() {
	size_t n = 0;
	struct val *v = stack;
	while (v) {
		n++;
		v = v->next;
	}
	return n;
}

struct val *peekn(size_t n) {
	struct val *v = stack;
	while (v && n) {
		v = v->next;
		n--;
	}
	return v;
}

void pushint(int64_t val) {
	struct val *v = malloc(sizeof *v);
	v->type = VAL_INT;
	v->u.ival = val;
	push(v);
}

void pushstr(char *str) {
	struct val *v = malloc(sizeof *v);
	v->type = VAL_STR;
	v->u.sval = strdup(str);
	push(v);
	/* printf("pushstr %zu\n", strlen(str)); */
}

void pushblob(struct blob *b) {
	struct val *v = malloc(sizeof *v);
	v->type = VAL_BLOB;
	v->u.bval = b;
	blob_ref(b);
	push(v);
	/* printf("pushblob %zu\n", v->u.bval->len); */
}

void pushvec(struct vec *vv) {
	struct val *v = malloc(sizeof *v);
	v->type = VAL_VEC;
	v->u.vval = vec_dup(vv);
	push(v);
}

int64_t popint() {
	struct val *v = pop();
	int64_t iv = v ? v->u.ival : 0;
	if (!v)
		return 0;
	if (v->type != VAL_INT) {
		free(v);
		return 0;
	}
	free(v);
	return iv;
}

char *popstr() {
	struct val *v = pop();
	char *s = v ? v->u.sval : NULL;
	if (!v)
		return NULL;
	if (v->type != VAL_STR) {
		free(v);
		return NULL;
	}
	free(v);
	return s;
}

int popstrbuf(char *buf, size_t len) {
	char *s = popstr();
	if (s) {
		strlcpy(buf, s, len);
		free(s);
	}
	return !!s;
}

struct blob *popblob() {
	struct val *v = pop();
	struct blob *b = v ? v->u.bval : NULL;
	if (!v)
		return NULL;
	if (v->type != VAL_BLOB) {
		free(v);
		return NULL;
	}
	free(v);
	return b;
}

struct vec *popvec() {
	struct val *v = pop();
	struct vec *vv = v ? v->u.vval : NULL;
	if (!v)
		return NULL;
	if (v->type != VAL_VEC) {
		free(v);
		return NULL;
	}
	free(v);
	return vv;
}

int hc2i(char c) {
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return c - '0';
}

void procstr(char *cmd) {
	cmd++;
	if (cmd[strlen(cmd) - 1] == '"')
		cmd[strlen(cmd) - 1] = '\0';
	pushstr(cmd);
}

void procblob(char *cmd) {
	cmd++;
	size_t sz = 0;
	char *ncmd = cmd;
	unsigned char *blob;
	size_t i = 0;
	while (*ncmd && *ncmd != ']')
		ncmd++;
	blob = malloc(ncmd - cmd);
	sz = ncmd - cmd;
	while (*cmd && *cmd != ']') {
		char d1 = *(cmd++);
		char d2 = *(cmd++);
		blob[i++] = (hc2i(d1) << 4) | hc2i(d2);
	}
	pushblob(blob_new_data(blob, i));
}

void procmulti(char *cmd) {
	cmd++;
	if (*cmd == '[')
		procblob(cmd);
}

void procint(char *cmd) {
	long val = strtol(cmd, NULL, 0);
	pushint(val);
}

void prval(size_t index, struct val *v, int pridx) {
	if (pridx)
		printf("%zu ", index);
	if (v->type == VAL_INT) {
		printf("int 0x%llx == %lld\n", v->u.ival, v->u.ival);
	} else if (v->type == VAL_STR) {
		printf("str '%s'\n", v->u.sval);
	} else if (v->type == VAL_BLOB) {
		char firstbytes[5];
		if (v->u.bval->len >= 2) {
			snprintf(firstbytes, sizeof(firstbytes), "%02x%02x",
			         v->u.bval->data[0],
			         v->u.bval->data[1]);
		} else if (v->u.bval->len == 1) {
			snprintf(firstbytes, sizeof(firstbytes), "%02x",
			         v->u.bval->data[0]);
		} else {
			snprintf(firstbytes, sizeof(firstbytes), "");
		}
		printf("blob %zu bytes, starts with '%s'\n",
		       v->u.bval->len, firstbytes);
	} else if (v->type == VAL_VEC) {
		char firstents[128];
		if (v->u.vval->filled >= 2) {
			snprintf(firstents, sizeof(firstents), "%llx, %llx",
			         v->u.vval->data[0], v->u.vval->data[1]);
		} else if (v->u.vval->filled == 1) {
			snprintf(firstents, sizeof(firstents), "%llx",
			         v->u.vval->data[0]);
		} else {
			snprintf(firstents, sizeof(firstents), "");
		}
		printf("vec %zu slots %zu filled, starts with '%s'\n",
		       v->u.vval->len, v->u.vval->filled, firstents);
	}
}

void cmd_dup() {
	struct val *v = peek();
	if (!v)
		return;
	if (v->type == VAL_INT) {
		pushint(v->u.ival);
	} else if (v->type == VAL_STR) {
		pushstr(v->u.sval);
	} else if (v->type == VAL_BLOB) {
		pushblob(v->u.bval);
	} else if (v->type == VAL_VEC) {
		pushvec(v->u.vval);
	}
}

void cmd_dump() {
	struct val *v = stack;
	size_t i = 0;
	while (v) {
		prval(i, v, 1);
		i++;
		v = v->next;
	}
}

void cmd_len() {
	struct val *v = pop();
	if (!v)
		return;
	if (v->type == VAL_STR)
		pushint(strlen(v->u.sval));
	else if (v->type == VAL_BLOB)
		pushint(v->u.bval->len);
	else if (v->type == VAL_VEC)
		pushint(v->u.vval->filled);
	/* XXX free v */
}

void cmd_print() {
	struct val *v = peek();
	if (!v)
		return;
	prval(0, v, 0);
}

void cmd_read() {
	struct stat st;
	char b[PATH_MAX];
	size_t sz;
	unsigned char *buf;
	struct blob *blob;
	int fd;

	if (!popstrbuf(b, sizeof b)) {
		warnx("read: needs string filename");
		return;
	}

	if (stat(b, &st)) {
		warn("stat(%s)", b);
		return;
	}

	buf = malloc(st.st_size);
	fd = open(b, O_RDONLY);
	if (fd == -1) {
		warn("open(%s)", b);
		free(buf);
		return;
	}

	read(fd, buf, st.st_size);
	close(fd);

	pushblob(blob_new_data(buf, st.st_size));
}

void cmd_write() {
	char b[PATH_MAX];
	struct blob *blob;
	int fd;
	if (!popstrbuf(b, sizeof b)) {
		warnx("write: needs str filename");
		return;
	}
	if (!(blob = popblob())) {
		warnx("write: needs blob");
		return;
	}
	if ((fd = open(b, O_WRONLY | O_TRUNC | O_CREAT, 0700)) == -1) {
		warn("open(%s)", b);
		return;
	}
	write(fd, blob->data, blob->len);
	close(fd);
	blob_unref(blob);
}

/* blob start len -> blob */
void cmd_slice() {
	int64_t len = popint();
	int64_t start = popint();
	struct blob *b = popblob();
	struct blob *b2;
	if (!b) {
		warnx("slice: needs blob");
		return;
	}
	if (start < 0)
		start = 0;
	if (start >= b->len)
		start = b->len - 1;
	if (len < 0)
		len = 0;
	if (len + start >= b->len)
		len = b->len - start;
	b2 = blob_new_view(b, start, len);
	blob_unref(b);
	pushblob(b2);
}

/* grep: blob|str blob|str -> vec */
void cmd_grep() {
	struct val *needle = pop();
	struct val *haystack = pop();

	unsigned char *hsbuf;
	unsigned char *ndbuf;
	size_t ndlen;
	size_t hslen;

	size_t hsidx;

	struct vec *result = vec_new(16);

	if (needle->type == VAL_STR) {
		ndbuf = (unsigned char *)needle->u.sval;
		ndlen = strlen(needle->u.sval);
	} else if (needle->type == VAL_BLOB) {
		ndbuf = needle->u.bval->data;
		ndlen = needle->u.bval->len;
	} else {
		warnx("grep: needs blob|str");
		/* XXX val_free(needle, haystack) */
		return;
	}

	if (haystack->type == VAL_STR) {
		hsbuf = (unsigned char *)haystack->u.sval;
		hslen = strlen(haystack->u.sval);
	} else if (haystack->type == VAL_BLOB) {
		hsbuf = haystack->u.bval->data;
		hslen = haystack->u.bval->len;
	} else {
		warnx("grep: needs blob|str");
		/* XXX val_free */
		return;
	}

	for (hsidx = 0; hsidx + ndlen < hslen; hsidx++)
		if (!memcmp(hsbuf + hsidx, ndbuf, ndlen))
			vec_add(result, hsidx);
	pushvec(result);
}

int64_t adler(unsigned char *data, size_t len) {
	uint32_t av;
	uint32_t a = 1, b = 0;
	size_t i;
	for (i = 0; i < len; i++) {
		a = (a + data[i]) % 65521;
		b = (b + a) % 65521;
	}
	return (b << 16) | a;
}

void cmd_adler() {
	struct blob *b = popblob();
	if (!b) {
		warnx("adler: needs blob");
		return;
	}
	pushint(adler(b->data, b->len));
	blob_unref(b);
}

struct {
	const char *name;
	void (*func)();
} cmds[] = {
	{ "adler", cmd_adler },
	{ "dup", cmd_dup },
	{ "dump", cmd_dump },
	{ "grep", cmd_grep },
	{ "len", cmd_len },
	{ "read", cmd_read },
	{ "print", cmd_print },
	{ "write", cmd_write },
	{ "slice", cmd_slice },
	{ NULL, NULL },
};

void proccmd(char *cmd) {
	size_t i;
	for (i = 0; cmds[i].name; i++) {
		if (!strcmp(cmd, cmds[i].name)) {
			cmds[i].func();
			return;
		}
	}
	printf("? %s\n", cmd);
}

void proctoken(char *token) {
	if (*token == '"')
		procstr(token);
	else if (*token == '#')
		procmulti(token);
	else if (isdigit(*token))
		procint(token);
	else
		proccmd(token);
}

void procline(char *line) {
	int instr = 0;
	char *start = line;
	while (*line) {
		if ((instr && *line == '"') || (!instr && *line == ' ')) {
			*line = '\0';
			if (line - start > 0)
				proctoken(start);
			start = line + 1;
			instr = 0;
		} else if (!instr && *line == '"') {
			instr = 1;
		}
		line++;
	}
	if (line - start > 1)
		proccmd(start);
}

int main(int argc, char *argv[]) {
	procline(argv[1]);
}
