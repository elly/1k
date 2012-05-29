/* match.c - matches a glob pattern against a string.
 * A glob pattern uses '?' and '*' like sh(1) globs.
 *
 * We use an approach modeled on Thompson's NFA approach from
 * <http://swtch.com/~rsc/regexp/regexp1.html> - we keep track of all the states
 * we might be in at any given time, which requires at most linear space, and we
 * declare victory if we are in any states at the end of 'str'.
 *
 * We keep two pointers, p and s; p is a pointer into the pattern space and s is
 * a pointer into the string space. Each time we 'step' the NFA, s increases,
 * and we compute the new set of possible values of p for the new value of s.
 * Since we advance one character down the string at each step of matching and
 * store a number of states at most equal to the length of the pattern, this
 * implementation requires linear space (with the size of the pattern) and
 * O(nm) time, where n = |str| and m = |pat|.
 *
 * Please send improvements and bugs to <elly+match@leptoquark.net>.
 */

#include <stdio.h>
#include <stdlib.h>

struct states {
	int n;
	char **p;
};

/* Add a single state to the supplied list. We skip adding it if it's already
 * present. */
static int pushstate(struct states *st, char *p) {
	int i;
	void *n;
	printf("  push %p %02x\n", p, (unsigned int)*p);
	for (i = 0; i < st->n; i++)
		if (st->p[i] == p)
			return 0;
	n = realloc(st->p, sizeof(*st->p) * ++st->n);
	if (!n)
		return -1;
	st->p = n;
	st->p[st->n - 1] = p;
	return 0;
}

/* Step the NFA one character forward. We construct a list of what our states
 * should be after inspecting this character and swap it out for the old list,
 * returning 0 for success. */
static int step(struct states *st, char c) {
	struct states nst = { 0, NULL };
	int i;
	printf("step %02x\n", (unsigned int)c);
	for (i = 0; i < st->n; i++) {
		char *p = st->p[i];
		printf("  look %p %02x\n", p, (unsigned int)*p);
		if (*p == '*') {
			if (pushstate(&nst, p))
				goto fail;
			if (pushstate(&nst, p + 1))
				goto fail;
		} else if (*p == '?' && c) {
			if (pushstate(&nst, p + 1))
				goto fail;
		} else if (*p == c) {
			if (pushstate(&nst, p + 1))
				goto fail;
		}
	}
	free(st->p);
	st->p = nst.p;
	st->n = nst.n;
	return 0;
fail:
	free(nst.p);
	return -1;
}

static int startstates(struct states *st, char *p) {
	st->n = 0;
	st->p = NULL;
	while (*p == '*')
		if (pushstate(st, p++))
			return -1;
	return pushstate(st, p);
}

/* We're in an end state if all we have left is stars. */
static int endstate(char *p) {
	while (*p)
		if (*p++ != '*')
			return 0;
	return 1;
}

/* Matches the supplied string against the supplied pattern. Returns 0 for match
 * failure, 1 for match success, and -1 for out of memory. */
int match(const char *pat, const char *str) {
	struct states ss = { 0, NULL };
	int i;
	if (startstates(&ss, (char *)pat))
		return -1;
	while (*str) {
		if (step(&ss, *(str++)))
			return -1;
		if (!ss.n)
			return 0;
	}
	for (i = 0; i < ss.n; i++) {
		if (endstate(ss.p[i]))
			break;
	}
	free(ss.p);
	return i != ss.n;
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Usage: %s <pat> <str>\n", argv[0]);
		return 1;
	}
	return match(argv[1], argv[2]);
}
