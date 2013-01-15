/* merkle.c - merkle hashing tool */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum {
	MAXHASH = 64
};

struct hasher {
	void *(*new)(void *aux);
	void (*free)(void *hash);

	void (*init)(void *hash);
	void (*update)(void *hash, const unsigned char *buf, size_t sz);
	void (*final)(void *hash, unsigned char *hashbuf);
	size_t size;
	void *aux;
};

struct merkle {
	struct merkle *parent;
	size_t size;
	size_t filled;
	struct hasher *hasher;
	void *hash;
};

struct merkle *merkle_new(size_t sz, struct hasher *hasher) {
	struct merkle *m = malloc(sizeof *m);
	m->parent = NULL;
	m->size = sz;
	m->filled = 0;
	m->hasher = hasher;
	m->hash = hasher->new(hasher->aux);
	hasher->init(m->hash);
	return m;
}

void merkle_update(struct merkle *m, const unsigned char *buf, size_t sz) {
	unsigned char hashbuf[MAXHASH];
	assert(!(m->size % sz));
	m->hasher->update(m->hash, buf, sz);
	m->filled += sz;
	if (m->filled != m->size)
		return;
	m->hasher->final(m->hash, hashbuf);
	if (!m->parent)
		m->parent = merkle_new(m->size, m->hasher);
	merkle_update(m->parent, hashbuf, m->hasher->size);
	m->hasher->init(m->hash);
	m->filled = 0;
}

void merkle_final(struct merkle *m, unsigned char *buf) {
	if (m->parent) {
		merkle_final(m->parent, buf);
		return;
	}
	m->hasher->final(m->hash, buf);
}

/* demo code starts here */

struct bogus_state {
	unsigned char n;
};

static void* bogus_new(void *aux) {
	struct bogus_state *s = malloc(sizeof *s);
	s->n = 0;
	return s;
}

static void bogus_free(void *p) {
	free(p);
}

static void bogus_init(void *p) {
	struct bogus_state *s = p;
	s->n = 0;
}

static void bogus_update(void *p, const unsigned char *n, size_t sz) {
	struct bogus_state *s = p;
	while (sz--)
		s->n += *n++;
}

static void bogus_final(void *p, unsigned char *buf) {
	struct bogus_state *s = p;
	buf[0] = s->n;
}

struct hasher bogus_hasher = {
	.new = bogus_new,
	.free = bogus_free,
	.init = bogus_init,
	.update = bogus_update,
	.final = bogus_final,
	.size = 1,
	.aux = NULL
};

int main(void) {
	unsigned char buf;
	int i;
	struct merkle *base = merkle_new(4, &bogus_hasher);
	for (i = 0; i < 500; i++) {
		buf = i;
		merkle_update(base, &buf, sizeof(buf));
	}
	merkle_final(base, &buf);
	printf("%d\n", buf);
}
