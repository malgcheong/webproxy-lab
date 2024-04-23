CC = gcc
CFLAGS = -O2 -Wall -I -g

all: echoclient echoserver

echoclient: echoclient.c ../csapp.c
	$(CC) $(CFLAGS) -o echoclient echoclient.c ../csapp.c

echoserver: echoserveri.c ../csapp.c
	$(CC) $(CFLAGS) -o echoserver echoserveri.c echo.c ../csapp.c

clean:
	rm -f *.o echo *~ echoclient *~ echoserver *~

