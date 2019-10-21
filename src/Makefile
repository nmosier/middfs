LIB_NAME = middfs-lib.a
LIB_DIR = lib
LIB = $(realpath $(LIB_DIR)/$(LIB_NAME))


.PHONY: all
all: server/middfs-server client/middfs-client

server/middfs-server: server/Makefile
	cd server && $(MAKE) middfs-server

client/middfs-client: client/Makefile
	cd client && $(MAKE) middfs-client


.PHONY: lib
lib: $(LIB)

$(LIB):
	cd $(LIB_DIR) && $(MAKE) $@

