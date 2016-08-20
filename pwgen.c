/* pwgen.c */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define PWLEN 10

int main() {
	int fd = open("/dev/urandom", O_RDONLY);
	char pwd[PWLEN + 1];
	unsigned char pwrand[PWLEN];
	static const char pwchars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789.-";
	int i;

	if (fd == -1)
		err(1, "open(/dev/urandom)");
	if (read(fd, pwrand, sizeof(pwrand)) != sizeof(pwrand))
		err(1, "read(/dev/urandom)");
	close(fd);

	for (i = 0; i < sizeof(pwrand); i++)
		pwd[i] = pwchars[pwrand[i] % (sizeof(pwchars) - 1)];
	pwd[i] = '\0';
	printf("%s\n", pwd);
	return 0;
}
