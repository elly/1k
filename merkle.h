/* merkle.h */

#ifndef MERKLE_H
#define MERKLE_H

enum {
	MAXHASH = 64
};

struct hasher {
	void *(*new)(void *aux);
	void (*free)(void *aux, void *hash);

	void (*init)(void *aux, void *hash);
	void (*update)(void *aux, void *hash, const unsigned char *buf, size_t sz);
	void (*final)(void *aux, void *hash, unsigned char *hashbuf);
	size_t size;
	void *aux;
};

struct merkle;

struct merkle *merkle_new(size_t sz, struct hasher *hasher);
void merkle_update(struct merkle *m, const unsigned char *buf, size_t sz);
void merkle_final(struct merkle *m, unsigned char *buf);

extern struct hasher md5_hasher;
extern struct hasher sha1_hasher;
extern struct hasher sha256_hasher;
extern struct hasher sha512_hasher;

#endif /* !MERKLE_H */
