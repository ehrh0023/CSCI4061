CC = gcc
CFLAGS = -D_REENTRANT
LDFLAGS = -lpthread

web_server: server.c
	${CC} -g -o web_server server.c util.o ${LDFLAGS} ${CFLAGS}

clean:
	rm web_server

