SRCS = middfs-client.c
OBJS = $(SRCS:.c=.o)
BIN = middfs-client

CFLAGS = -c `pkg-config fuse3 --cflags` -g
LDLIBS = `pkg-config fuse3 --libs`

# TODO -- C header file dependencies

$(BIN): $(OBJS)
	$(CC) $(LDLIBS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

