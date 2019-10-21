# Predefined Environment/Makefile Variables
#  LIB -- path to library

SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h)
OBJS = $(SRCS:.c=.o)

.PHONY: all
all: $(LIB)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $<

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

.PHONY: clean
clean:
	rm -f $(OBJS) $(LIB)


