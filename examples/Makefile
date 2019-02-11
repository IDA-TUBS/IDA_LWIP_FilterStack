CC=gcc

SRCDIR = ./src
INCDIR = ./include
LWIPDIR = ../lwip-2.1.2/src
LIBDIR = ../lib
PORTDIR = ../ports/os/unix

SOURCES := \
	$(SRCDIR)/netif/tapif.c \
	$(SRCDIR)/test.c \
	$(SRCDIR)/default_netif.c \
	$(PORTDIR)/perf.c \
	$(PORTDIR)/sys_arch.c 
	
OBJ = $(SOURCES:%.c=%.o)

TARGET = example

LDFLAGS+=-lutil -lpthread -llwipcore_unix_none
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

