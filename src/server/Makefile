# middfs-server Makefile

SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h)
OBJS = $(SRCS:.c=.o)
BIN  = middfs-server

# Default Running Parameters

# Default Target
.PHONY: all
all:
	cd .. && $(MAKE) server

$(BIN): $(OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f $(OBJS) $(BIN)

.PHONY: attach
attach:
	gdb $(BIN) `pgrep $(BIN)`

.PHONY: kill
kill:
	kill -TERM `pgrep $(BIN)`

.PHONY: mount
mount:
	$(realpath $(BIN)) 1

.PHONY: debug
debug:
	gdb --args $(realpath $(BIN)) 1

.PHONY: mem
mem:
	valgrind $(realpath $(BIN)) 1

