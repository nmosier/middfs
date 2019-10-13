# middfs-server Makefile

SRCS = middfs-server.c middfs-server-conn.c
HDRS = middfs-server-conn.h
OBJS = $(SRCS:.c=.o)
BIN  = middfs-server
CFLAGS = -c -g -Wall -pedantic -I..

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
