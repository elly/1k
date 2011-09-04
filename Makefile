CFLAGS := -Wall -Wextra -g
PROGS := httpd

all : $(PROGS)

httpd : httpd.o

clean :
	rm -f $(PROGS) *.o
