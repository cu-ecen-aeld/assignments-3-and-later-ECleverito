CROSS_COMPILER=
CC=${CROSS_COMPILER}gcc

CCFLAGS=-Werror -Wall -O0 -g -pthread

all: aesdsocket

aesdsocket:	aesdsocket.c aesdsocket.h
	${CC} ${CCFLAGS} aesdsocket.c -o aesdsocket

clean:
	rm -f *.o aesdsocket