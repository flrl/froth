TARGET=froth

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)

CFLAGS += -g -Wall -std=c99
LDFLAGS :=

.PHONY : all clean depends

all : $(TARGET)

$(TARGET) : $(OBJS)

clean :
	$(RM) $(OBJS) $(TARGET) core

realclean : clean
	$(RM) $(DEPS)

depends : $(DEPS)


define DEPEND_template
 $(1).d : $(1).c
	$$(CC) $$(CFLAGS) -MM -MF $(1).d -MT $(1).o $(1).c
endef
#$(foreach depend,$(basename $(DEPS)),$(eval $(call DEPEND_template,$(depend))))
$(foreach depend,$(basename $(DEPS)),$(eval $(call DEPEND_template,$(depend))))

#-include $(DEPS)
