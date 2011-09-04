/* bf_rt.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BF_SIZE		4096

extern void bf_main(char *b, int sz);

extern void bfrt_write(unsigned long v) {
	putchar(v);
}

extern unsigned long bfrt_read(void) {
	return getchar();
}

int main(int argc, char *argv[]) {
	char *b = malloc(BF_SIZE * 8);
	memset(b, 0, BF_SIZE);
	bf_main(b, BF_SIZE);
	return 0;
}
