CFLAGS := -Wall -Wextra -g
PROGS := fmt.o httpd sdate

all: $(PROGS)

fth: fth.S
	clang -static -nostdlib -o $@ $^

clean :
	rm -f $(PROGS) *.o
