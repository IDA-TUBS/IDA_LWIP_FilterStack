OS=ucos-ii
CPU=psur5

LWIPOPTSDIR=./
OBJDIR := ./obj
LIBDIR := ./lib
LIB = $(LIBDIR)/liblwipcore_$(OS)_$(CPU).a

LWIPDIR = ./lwip-2.1.2/src
-include $(LWIPDIR)/Filelists.mk

INC = -I$(LWIPDIR)/include -I $(LWIPOPTSDIR)

#check for OS type
ifeq ($(OS),ucos-ii)
INC += -I ./ports/os/ucos_ii/
else ifeq ($(OS),unix)
INC += -I ./ports/os/unix/
else
INC += -I ./ports/os/FreeRTOS/
endif

#check for CPU type
ifeq ($(CPU),psur5)
CFLAGS := -mcpu=cortex-r5  -g3 -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -MP -MMD -std=gnu11
CC := arm-none-eabi-gcc
AR := arm-none-eabi-ar
else ifeq ($(CPU),cortex-m7)
CFLAGS := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard -g3 -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -MP -MMD -std=gnu11
CC := arm-none-eabi-gcc
AR := arm-none-eabi-ar
endif

OBJECTS := $(LWIPNOAPPSFILES:$(LWIPDIR)/%.c=$(OBJDIR)/%.o)

$(OBJECTS): $(OBJDIR)/%.o : $(LWIPDIR)/%.c
	@echo 'Building file: $<'
	@mkdir -p $(@D)
	@$(CC) -c $(CFLAGS) $(INC) -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o $@ $< >/dev/null

$(LIB) : $(OBJECTS)
	@echo 'Linking: $@'
	@mkdir -p $(@D)
	@$(AR) -r -o $@ $^ >/dev/null

all: $(LIB)

clean :
	rm -rf $(OBJDIR)
	rm -rf $(LIBDIR)

help:
	@echo 'Default Arguments: CPU=psur5 OS=ucos-ii'
	@echo 'CPU={psur5,cortex-m7}'
	@echo 'OS={ucos-ii,unix}'
