FUSE ?= 3 # specify FUSE major version

ifeq ($(FUSE),3)
	FUSE_LIBNAME = fuse3
else
	FUSE_LIBNAME = fuse
endif

SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h)
OBJS = $(SRCS:.c=.o)

BIN ?= middfs-client

CFLAGS += `pkg-config $(FUSE_LIBNAME) --cflags`
LDLIBS += `pkg-config $(FUSE_LIBNAME) --libs`
CPPFLAGS += -DFUSE=$(FUSE)

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
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $<

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
