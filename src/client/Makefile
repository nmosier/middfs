SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h)
OBJS = $(SRCS:.c=.o)

BIN ?= middfs-client

CFLAGS += `pkg-config fuse3 --cflags`
LDLIBS += `pkg-config fuse3 --libs`

# Default Mount Parameters
ROOT_DIR = tst
CLIENT_DIR = home
CLIENT_NAME = client
FUSE_ARGS = -s -f # run in foreground & single-threaded
CLIENT_ARGS = $(FUSE_ARGS) --root="$(ROOT_DIR)" --dir="$(CLIENT_DIR)" --name="$(CLIENT_NAME)" "$(ROOT_DIR)"

# TODO -- C header file dependencies

$(BIN): $(OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f $(OBJS) $(BIN)

.PHONY: mount
mount:
	$(realpath $(BIN)) $(CLIENT_ARGS)

.PHONY: umount
umount:
	umount "$(ROOT_DIR)"

.PHONY: gdb-mount
gdb-mount:
	gdb $(realpath $(BIN)) --args $(realpath $(BIN)) $(CLIENT_ARGS)

.PHONY: attach
attach:
	gdb $(BIN) `pgrep middfs-client`

.PHONY: kill
kill:
	kill -KILL `pgrep middfs-client`
