TARGET=froth

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)
GENS := builtin.h

CFLAGS += -g -Wall -std=c99
LDFLAGS :=

.PHONY : all clean depends

all : $(TARGET)

builtin.h : builtin.c
	perl genh _BUILTIN_H $^ > $@

$(TARGET) : $(OBJS)

clean :
	$(RM) $(OBJS) $(GENS) $(TARGET) core

realclean : clean
	$(RM) $(DEPS)

depends : $(DEPS)

define DEPEND_template
 $(1).d : $(1).c $(1).h
	$$(CC) $$(CFLAGS) -MM -MF $(1).d -MT $(1).o $(1).c
endef
$(foreach depend,$(basename $(DEPS)),$(eval $(call DEPEND_template,$(depend))))

-include $(DEPS)
