OS=ucos-ii
CC=gcc

SRCDIR = ./src
INCDIR = ./include
LWIPDIR = ../lwip-2.1.2/src
LIBDIR = ../lib
OBJDIR= ./obj
PORTDIR = ../ports/os/unix

SOURCES = \
	netif/tapif.c \
	test.c \
	default_netif.c \
	$(PORTDIR)/perf.c \
	$(PORTDIR)/sys_arch.c 
	
OBJ = $(OBJDIR)/perf.o \
	$(OBJDIR)/sys_arch.o \
	$(OBJDIR)/tapif.o \
	$(OBJDIR)/test.o \
	$(OBJDIR)/default_netif.o	
	
TARGET = test	

include $(LWIPDIR)/Filelists.mk
ARCHFILES = $(PORTDIR)/perf.c \
			$(PORTDIR)/sys_arch.c \
			$(TESTDIR)/netif/tapif.c 
LWIPFILES=$(LWIPNOAPPSFILES) $(ARCHFILES)
LWIPLIBCOMMON=../lib/liblwipcore_unix_none.a

LDFLAGS+=-lutil

CFLAGS= -Wall -g3 -pthread 

INC= -I ./ \
	-I $(INCDIR) \
	-I $(LWIPDIR)/include/ \
	-I $(LWIPDIR)/api/ \
	-I $(LWIPDIR)/core/ \
	-I $(PORTDIR) \

$(TARGET): $(OBJ) $(LWIPLIBCOMMON)
	@echo 'Linking obj: $@'
	@$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LWIPLIBCOMMON) $(LDFLAGS) $(INC)

all: $(TARGET)
	
$(OBJDIR)/test.o: $(LWIPLIBCOMMON)
	$(CC) $(CFLAGS) -c $(SRCDIR)/test.c $(INC) -o $@
$(OBJDIR)/default_netif.o:
	$(CC) $(CFLAGS) -c $(SRCDIR)/default_netif.c $(INC)	-o $@
$(OBJDIR)/tapif.o:
	$(CC) $(CFLAGS) -c $(SRCDIR)/netif/tapif.c $(INC) -o $@
$(OBJDIR)/perf.o:
	$(CC) $(CFLAGS) -c $(PORTDIR)/perf.c $(INC)	-o $@
$(OBJDIR)/sys_arch.o:
	$(CC) $(CFLAGS) -c $(PORTDIR)/sys_arch.c $(INC) -o $@


clean:
	rm -rf $(OBJ) test

