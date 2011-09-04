CFLAGS := -Wall -Wextra -g
PROGS := httpd sdate

all : $(PROGS)

clean :
	rm -f $(PROGS) *.o
