TARGET=froth

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)
GENS := builtin.h

CFLAGS += -g -Wall -std=c99
LDFLAGS :=

.PHONY : all clean depends realclean

all : $(TARGET)

builtin.h : builtin.c
	-perl dictcheck builtin.c
	perl genh _BUILTIN_H $^ > $@

$(TARGET) : $(OBJS)

$(DEPS) : $(GENS)

clean :
	$(RM) $(OBJS) $(GENS) $(TARGET) core

depends : $(DEPS)

realclean : clean
	$(RM) $(DEPS)

define DEPEND_template
 $(1).d : $(1).c
	$$(CC) $$(CFLAGS) -MM -MF $(1).d -MT $(1).o $(1).c
endef
$(foreach depend,$(basename $(DEPS)),$(eval $(call DEPEND_template,$(depend))))

-include $(DEPS)
