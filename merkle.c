/* merkle.c - merkle hashing tool
 * Used like: merkle <hash type> <block size>
 * hashes stdin, emits hash (as hex) on stdout
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

#include "merkle.h"

struct merkle {
	struct merkle *parent;
	size_t size;
	size_t filled;
	struct hasher *hasher;
	void *hash;
};

struct merkle *merkle_new(size_t sz, struct hasher *hasher) {
	struct merkle *m = malloc(sizeof *m);
	assert(sz > hasher->size);
	assert(!(sz % hasher->size));
	m->parent = NULL;
	m->size = sz;
	m->filled = 0;
	m->hasher = hasher;
	m->hash = hasher->new(hasher->aux);
	hasher->init(hasher->aux, m->hash);
	return m;
}

void merkle_update(struct merkle *m, const unsigned char *buf, size_t sz) {
	unsigned char hashbuf[MAXHASH];
	assert(!(m->size % sz));
	m->hasher->update(m->hasher->aux, m->hash, buf, sz);
	m->filled += sz;
	if (m->filled != m->size)
		return;
	m->hasher->final(m->hasher->aux, m->hash, hashbuf);
	if (!m->parent)
		m->parent = merkle_new(m->size, m->hasher);
	merkle_update(m->parent, hashbuf, m->hasher->size);
	m->hasher->init(m->hasher->aux, m->hash);
	m->filled = 0;
}

void merkle_final(struct merkle *m, unsigned char *buf) {
	if (m->parent) {
		merkle_final(m->parent, buf);
		return;
	}
	m->hasher->final(m->hasher->aux, m->hash, buf);
}

/* demo code starts here */

void *evpmd_new(void *aux) {
	return EVP_MD_CTX_create();
}

void evpmd_free(void *aux, void *hash) {
	EVP_MD_CTX_destroy(hash);
}

void evpmd_init(void *aux, void *hash) {
	EVP_MD *(*func)(void) = aux;
	EVP_DigestInit(hash, func());
}

void evpmd_update(void *aux, void *hash, const unsigned char *buf, size_t sz) {
	EVP_DigestUpdate(hash, buf, sz);
}

void evpmd_final(void *aux, void *hash, unsigned char *buf) {
	unsigned int ignored = MAXHASH;
	EVP_DigestFinal_ex(hash, buf, &ignored);
}

struct hasher md5_hasher = {
	.new = evpmd_new,
	.free = evpmd_free,
	.init = evpmd_init,
	.update = evpmd_update,
	.final = evpmd_final,
	.size = 16,
	.aux = EVP_md5
};

struct hasher sha1_hasher = {
	.new = evpmd_new,
	.free = evpmd_free,
	.init = evpmd_init,
	.update = evpmd_update,
	.final = evpmd_final,
	.size = 20,
	.aux = EVP_sha1
};

struct hasher sha256_hasher = {
	.new = evpmd_new,
	.free = evpmd_free,
	.init = evpmd_init,
	.update = evpmd_update,
	.final = evpmd_final,
	.size = 32,
	.aux = EVP_sha256
};

struct hasher sha512_hasher = {
	.new = evpmd_new,
	.free = evpmd_free,
	.init = evpmd_init,
	.update = evpmd_update,
	.final = evpmd_final,
	.size = 64,
	.aux = EVP_sha512
};

struct {
	const char *name;
	struct hasher *hasher;
} hashers[] = {
	{ "md5", &md5_hasher },
	{ "sha1", &sha1_hasher },
	{ "sha256", &sha256_hasher },
	{ "sha512", &sha512_hasher },
	{ NULL, NULL }
};

int main(int argc, char *argv[]) {
	static int blocksize = 1024;
	unsigned char *buf;
	int i;
	struct merkle *base;
	struct hasher *hasher = &sha256_hasher;

	if (argc > 1)
		for (i = 0; hashers[i].name; i++)
			if (!strcmp(argv[1], hashers[i].name))
				hasher = hashers[i].hasher;

	if (argc > 2)
		blocksize = atoi(argv[2]);

	buf = malloc(blocksize);
	base = merkle_new(blocksize, hasher);

	while ((i = read(0, buf, blocksize)) > 0) {
		if (i < blocksize)
			memset(buf + i, 0, blocksize - i);
		merkle_update(base, buf, blocksize);
	}
	merkle_final(base, buf);
	for (i = 0; i < sha256_hasher.size; i++)
		printf("%02x", buf[i]);
	printf("\n");
	return 0;
}
