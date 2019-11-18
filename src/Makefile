# FUSE_USE_VERSION ?= 31 # specify default FUSE API

LIB_DIR = lib
SERVER_DIR = server
CLIENT_DIR = client

LIB_NAME = middfs-lib.a
SERVER_NAME = middfs-server
CLIENT_NAME = middfs-client

LIB := $(realpath $(LIB_DIR))/$(LIB_NAME)
SERVER := $(realpath $(SERVER_DIR))/$(SERVER_NAME)
CLIENT := $(realpath $(CLIENT_DIR))/$(CLIENT_NAME)
BINS := $(SERVER) $(CLIENT)

export CFLAGS := -c -Wall -pedantic -g -I $(realpath .)
export LDLIBS := $(LIB)

.PHONY: all
all: $(BINS)

.PHONY: lib
lib: $(LIB)

.PHONY: server
server: $(SERVER)

.PHONY: client
client: $(CLIENT)

$(LIB): FORCE
	cd $(LIB_DIR) && $(MAKE) LIB=$(LIB) $@

$(SERVER): $(LIB) FORCE
	cd $(SERVER_DIR) && $(MAKE) BIN=$(SERVER) $@

$(CLIENT): $(LIB) FORCE
	cd $(CLIENT_DIR) && $(MAKE) BIN=$(CLIENT) $@


FORCE: ;
