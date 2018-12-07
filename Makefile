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
else
INC += -I ./ports/os/FreeRTOS/
endif

#check for CPU type
ifeq ($(CPU),psur5)
CFLAGS := -mcpu=cortex-r5 -mthumb -mthumb-interwork -mfloat-abi=softfp -mfpu=neon -g3 -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -MP -MMD -std=gnu11 -fPIC
CC := arm-none-eabi-gcc
AR := arm-none-eabi-ar
else

endif

OBJECTS := $(LWIPNOAPPSFILES:$(LWIPDIR)/%.c=$(OBJDIR)/%.o)

$(OBJECTS): $(OBJDIR)/%.o : $(LWIPDIR)/%.c
	@echo 'Building file: $<'
	@mkdir -p $(@D)
	@$(CC) -c $(CFLAGS) $(INC) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o $@ $< >/dev/null

$(LIB) : $(OBJECTS)
	@echo 'Linking: $@'
	@mkdir -p $(@D)
	@$(AR) -r -o $@ $^ >/dev/null

all: $(LIB)

clean :
	rm -rf $(OBJDIR)
	rm -rf $(LIBDIR)
