SRCS = middfs-client.c
OBJS = $(SRCS:.c=.o)
BIN = middfs-client

CFLAGS = -c `pkg-config fuse3 --cflags` -g
LDLIBS = `pkg-config fuse3 --libs`

# Default Mount Parameters
ROOT_DIR=tst
CLIENT_DIR=home

# TODO -- C header file dependencies

$(BIN): $(OBJS)
	$(CC) $(LDLIBS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean
	rm -f $(OBJS) $(BIN)

.PHONY: mount
mount: $(BIN)
	sudo ./$(BIN) --root="$(ROOT_DIR)" --dir="$(CLIENT_DIR)" "$(ROOT_DIR)"

.PHONY: umount
umount:
	sudo umount "$(ROOT_DIR)"

.PHONY: attach
attach:
	sudo gdb $(BIN) `pgrep middfs`
