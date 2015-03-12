CC = gcc
CFLAGS = -D_REENTRANT
LDFLAGS = -lpthread

all:	web_server

web_server:	server.o util.o
	${CC} ${LDFLAGS} -o web_server server.o util.o

server.o:	server.c

clean:
	rm *.o web_server


