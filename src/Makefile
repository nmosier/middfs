SRCS = middfs-client.c middfs-client-ops.c middfs-client-util.c
HDRS = middfs-client.h middfs-client-ops.h middfs-client-util.h
OBJS = $(SRCS:.c=.o)
BIN = middfs-client

CFLAGS = -c `pkg-config fuse3 --cflags` -g -Wall -pedantic
LDLIBS = `pkg-config fuse3 --libs`

# Default Mount Parameters
ROOT_DIR=tst
CLIENT_DIR=home
CLIENT_NAME=client

# TODO -- C header file dependencies

$(BIN): $(OBJS)
	$(CC) $(LDLIBS) -o $@ $^

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f $(OBJS) $(BIN)

.PHONY: mount
mount: $(BIN)
	sudo ./$(BIN) --root="$(ROOT_DIR)" --dir="$(CLIENT_DIR)" --name="$(CLIENT_NAME)" "$(ROOT_DIR)"

.PHONY: umount
umount:
	sudo umount "$(ROOT_DIR)"

.PHONY: attach
attach:
	sudo gdb $(BIN) `pgrep middfs`
