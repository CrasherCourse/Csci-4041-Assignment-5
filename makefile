# Makefile for assignment 5

server: server.o
	gcc -g -o server server.o -lpthread

server.o: quote_server.c
	gcc -g -o server.o -c quote_server.c -lpthread

clean:
	rm *.o
	rm server
