SRCS = middfs-client.c middfs-client-ops.c middfs-client-rsrc.c ../middfs-rsrc.c ../middfs-util.c ../middfs-serial.c
HDRS = middfs-client.h middfs-client-ops.h middfs-client-rsrc.h ../middfs-serial.h
OBJS = $(SRCS:.c=.o)
BIN = middfs-client

CFLAGS = -c `pkg-config fuse3 --cflags` -g -Wall -pedantic -I..
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
	./$(BIN) --root="$(ROOT_DIR)" --dir="$(CLIENT_DIR)" --name="$(CLIENT_NAME)" "$(ROOT_DIR)"

.PHONY: umount
umount:
	umount "$(ROOT_DIR)"

.PHONY: attach
attach:
	gdb $(BIN) `pgrep middfs`

.PHONY: kill
kill:
	kill -TERM `pgrep middfs`
