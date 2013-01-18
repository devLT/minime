CFLAGS+= -g -std=c99 -pedantic -Wall
LDADD+= -lX11
LDFLAGS=
EXEC=minime

PREFIX?= /usr/local
BINDIR?= $(PREFIX)/bin

CC=gcc

SRC = minime.c
OBJ = ${SRC:.c=.o}

all: $(EXEC)
${EXEC}: ${OBJ}

	$(CC) $(LDFLAGS) -s -O2 -ffast-math -fno-unit-at-a-time -o $@ ${OBJ} $(LDADD)

install: all
	install -Dm 755 minime $(DESTDIR)$(BINDIR)/minime

clean:
	rm -fv minime *.o

