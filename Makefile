CC ?= cc
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L -D_DARWIN_C_SOURCE
CFLAGS ?= -std=c99 -O2 -Wall -Wextra

RAFT_C_ROOT ?= /Users/josem/src/consulting/raft_rx/c
RXNET_C_ROOT ?= /Users/josem/src/consulting/rxnet/c

INCLUDES := -Iinclude -I$(RAFT_C_ROOT)/include -I$(RXNET_C_ROOT)/include
RAFT_LIB := $(RAFT_C_ROOT)/build/libraft_rx.a
RXNET_LIB := $(RXNET_C_ROOT)/build/librxnet.a

BUILD := build
TARGET := $(BUILD)/rps_node

CORE_SRC := src/rps_game.c src/rps_command.c src/rps_app.c
NODE_SRC := src/main.c src/rps_args.c src/rps_cli.c
TEST_SRC := tests/test_hash_command.c tests/test_game.c tests/test_app.c
TEST_BINS := $(TEST_SRC:tests/%.c=$(BUILD)/%)

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(RAFT_LIB):
	$(MAKE) -C $(RAFT_C_ROOT) build/libraft_rx.a

$(RXNET_LIB):
	$(MAKE) -C $(RXNET_C_ROOT) build/librxnet.a

$(TARGET): $(BUILD) $(CORE_SRC) $(NODE_SRC) $(RAFT_LIB) $(RXNET_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $(CORE_SRC) $(NODE_SRC) $(RAFT_LIB) $(RXNET_LIB) -lpthread

$(BUILD)/test_%: tests/test_%.c $(CORE_SRC) $(RAFT_LIB) $(RXNET_LIB) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $< $(CORE_SRC) $(RAFT_LIB) $(RXNET_LIB) -lpthread

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo $$t; $$t; done

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
