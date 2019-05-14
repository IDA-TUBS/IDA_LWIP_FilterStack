CC=gcc
VER=restruct

SRCDIR = ./src
INCDIR = ./include
LWIPDIR = ../lwip-2.1.2/src
LIBDIR = ../lib
PORTDIR = ../ports/os/unix

SOURCES := \
	$(SRCDIR)/netif/tapif.c \
	$(SRCDIR)/test.c \
	$(SRCDIR)/udpecho.c \
	$(SRCDIR)/default_netif.c \
	$(PORTDIR)/perf.c \
	$(PORTDIR)/sys_arch.c 
	
OBJ = $(SOURCES:%.c=%.o)

TARGET = example

ifeq ($(VER),restruct)
LDFLAGS+=-lutil -lpthread -llwipcore_unix_none_restruct
else
LDFLAGS+=-lutil -lpthread -llwipcore_unix_none_normal
endif

CFLAGS= -Wall -g3 -pthread

INC= -I ./ \
	-I $(INCDIR) \
	-I $(LWIPDIR)/include/ \
	-I $(LWIPDIR)/api/ \
	-I $(LWIPDIR)/core/ \
	-I $(PORTDIR) \

$(TARGET): $(OBJ)
	@echo 'Linking: $@'
	@$(CC) -o $@ $(OBJ) -L$(LIBDIR) $(LDFLAGS)

all: $(TARGET)
	
$(OBJ): %.o: %.c
	@echo 'Building file: $<'
	@$(CC) -c $(CFLAGS) $(INC) -o $@ $<	

clean:
	rm -rf $(OBJ) $(TARGET)

