CROSS_COMPILE=
ifeq ($(CC),)
	CC=${CROSS_COMPILE}gcc
endif
ifeq ($(CFLAGS),)
	CFLAGS=-Werror -Wall -O0 -g -pthread
endif
ifeq ($(LDFLAGS),)
	LDFLAGS=-pthread -lrt
endif
all: aesdsocket

aesdsocket:	aesdsocket.c aesdsocket.h
	${CC} ${CFLAGS} ${LDFLAGS} aesdsocket.c -o aesdsocket

clean:
	rm -f *.o aesdsocket
