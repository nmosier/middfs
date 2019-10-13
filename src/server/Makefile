# middfs-server Makefile

SRCS = middfs-server.c middfs-server-conn.c
HDRS =
OBJS = $(SRCS:.c=.o)
BIN  = middfs-server

CFLAGS = -c -g -Wall -pedantic

# Default Running Parameters

$(BIN): $(OBJS)
	$(CC) -o $@ $^

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
