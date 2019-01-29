SRCDIR = ./src
INCDIR = ./include
LWIPDIR := ../lwip-2.1.2/src
PORTDIR := ../ports/os/unix
OJCDIR:= ../obj

CC=gcc
CFLAGS= -I. \
	-I $(INCDIR) \
	-I $(LWIPDIR)/include/ \
	-I $(LWIPDIR)/api/ \
	-I $(LWIPDIR)/core/ \
	-I $(PORTDIR) \


all: test.o tapif.o default_netif.o

test.o: $(SRCDIR)/test.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/test.c


default_netif.o: $(SRCDIR)/default_netif.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/default_netif.c
	
tapif.o: $(SRCDIR)/netif/tapif.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/netif/tapif.c
	
clean:
	rm -rf *.o

