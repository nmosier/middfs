SRCS = hello.c
OBJS = $(SRCS:.c=.o)
BIN = hello

CFLAGS = -c `pkg-config fuse3 --cflags`
LDLIBS = `pkg-config fuse3 --libs`

# TODO -- C header file dependencies

$(BIN): $(OBJS)
	$(CC) $(LDLIBS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

