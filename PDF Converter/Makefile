# To compile, type "make" or make "all"
# To remove files, type "make clean"

CC = gcc
CFLAGS = -Wall
OBJS = server.o client.o admin.o
LIBS = -lpthread

.SUFFIXES: .c .o 

all: server client admin

server: server.o
	$(CC) $(CFLAGS) -o server server.o $(LIBS)

client: client.o
	$(CC) $(CFLAGS) -o client client.o

admin: admin.o
	$(CC) $(CFLAGS) -o admin admin.o

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm -f $(OBJS) server client admin
