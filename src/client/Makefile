SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h)
OBJS = $(SRCS:.c=.o)

BIN ?= middfs-client

CFLAGS += `pkg-config fuse3 --cflags`
LDLIBS += `pkg-config fuse3 --libs`

# Default Mount Parameters
ROOT_DIR=tst2
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
mount:
	$(realpath $(BIN)) --root="$(ROOT_DIR)" --dir="$(CLIENT_DIR)" --name="$(CLIENT_NAME)" "$(ROOT_DIR)"

.PHONY: umount
umount:
	umount "$(ROOT_DIR)"

.PHONY: attach
attach:
	gdb $(BIN) `pgrep middfs-client`

.PHONY: kill
kill:
	kill -KILL `pgrep middfs-client`
