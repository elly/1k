/* qalloc.c */

#include "qalloc.h"

#define NULL			(void*)0
#define FFREE			0x00000001
#define MINBSZ		4

#define SZ(ptr)		*((unsigned*)ptr)
#define BSZ(p)		(SZ(p) & ~3)
#define NX(ptr)		(void*)((char*)ptr + BSZ(ptr))
#define ALIGN(s)	((s + 3) & ~3)
#define RNDSZ(s)	(ALIGN(s) + 4)

#define ISFREE(p)	(SZ(p) & FFREE)
#define FREE(p)		SZ(p) |= FFREE
#define ALLOC(p)	SZ(p) &= ~FFREE;

static struct {
	void *start;
	void *end;
} arena;

static void qjoin();

void qinit(void *start, unsigned size) {
	arena.start = start;
	SZ(start) = size;
	FREE(start);
	arena.end = (char*)start + size;
}

void *qalloc(unsigned size) {
	void *p = arena.start;
	void *n = NULL;

	size = RNDSZ(size);

	while (p && p < arena.end) {
		if (!ISFREE(p) || BSZ(p) < size) {
			p = NX(p);
			continue;
		}

		if (BSZ(p) > size) {
			n = (char*)p + size;
			SZ(n) = SZ(p) - size;
			SZ(p) = size;
		}
		
		ALLOC(p);
		return p;
	}
	return NULL;
}

void qfree(void *ptr) {
	FREE(ptr);
	qjoin();
}
static void qjoin() {
	void *p = arena.start;
	while (p && p < arena.end) {
		if (NX(p) < arena.end && ISFREE(p) && ISFREE(NX(p))) {
			SZ(p) += BSZ(NX(p));
		} else {
			p = NX(p);
		}
	}
}
