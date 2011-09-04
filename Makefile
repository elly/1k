CFLAGS := -Wall -Wextra -g
PROGS := fmt.o httpd sdate

all : $(PROGS)

clean :
	rm -f $(PROGS) *.o
