# sic - simple irc client

include config.mk

SRC = sic0.c
OBJ = ${SRC:.c=.o}

all: sic0

.c.o:
	@echo CC $<
	${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

sic0: ${OBJ}
	@echo CC $@
	${CC} $(CFLAGS) -o $@ ${OBJ} ${LDFLAGS}

clean:
	@rm -f sic0 ${OBJ} sic-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p sic-${VERSION}
	@cp -R LICENSE Makefile README config.mk sic.1 ${SRC} sic-${VERSION}
	@tar -cf sic-${VERSION}.tar sic-${VERSION}
	@gzip sic-${VERSION}.tar
	@rm -rf sic-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f sic ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/sic
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < sic.1 > ${DESTDIR}${MANPREFIX}/man1/sic.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/sic.1

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/sic
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/sic.1

.PHONY: all clean dist install uninstall
