/* codec.c */

#include <stdio.h>
#include <string.h>

void hex(char c) {
	static const char hexchars[] = "0123456789abcdef";
	printf("%c%c", hexchars[(c >> 4) & 0xf], hexchars[c & 0xf]);
}

void rot13(char c) {
	static const char rot13tab[] = "nopqrstuvwxyzabcdefghijklm";
	if (c >= 'a' && c <= 'z')
		c = rot13tab[c - 'a'];
	else if (c >= 'A' && c <= 'Z')
		c = rot13tab[c - 'A'] + 'A' - 'a';
	fwrite(&c, 1, 1, stdout);
}

int unhexc(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

void unhex(char c) {
	static int prev = -1;
	int cv = unhexc(c);
	char h;
	if (cv == -1)
		return;
	if (prev == -1) {
		prev = cv;
		return;
	}
	h = (prev << 4) | cv;
	fwrite(&h, 1, 1, stdout);
	prev = -1;
}

typedef void (*codecfn)(char);

struct {
	const char *name;
	codecfn func;
} codecs[] = {
	{ "hex", hex },
	{ "rot13", rot13 },
	{ "unhex", unhex },
	{ NULL, NULL },
};

codecfn findcodec(const char *n) {
	int i;
	for (i = 0; codecs[i].name; i++)
		if (!strcmp(codecs[i].name, n))
			return codecs[i].func;
	return NULL;
}

int main(int argc, char *argv[]) {
	int c;
	void (*codec)(char c);
	if (argc != 2)
		return 1;
	codec = findcodec(argv[1]);
	if (!codec)
		return 2;
	while ((c = getc(stdin)) != EOF)
		codec(c);
	return 0;
}
