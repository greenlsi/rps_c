CC ?= cc
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L -D_DARWIN_C_SOURCE
CFLAGS ?= -std=c99 -O2 -Wall -Wextra

RAFT_C_ROOT ?= /opt/homebrew

INCLUDES := -Iinclude -I$(RAFT_C_ROOT)/include 
LIBS := -L$(RAFT_C_ROOT)/lib -lraft_rx -lrxnet

BUILD := build
TARGET := $(BUILD)/rps_node

CORE_SRC := src/rps_game.c src/rps_command.c src/rps_app.c src/sha256.c
NODE_SRC := src/main.c src/rps_args.c src/rps_cli.c
TEST_SRC := tests/test_sha256.c tests/test_hash_command.c tests/test_game.c tests/test_app.c
TEST_BINS := $(TEST_SRC:tests/%.c=$(BUILD)/%) $(BUILD)/test_cli_status

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(BUILD) $(CORE_SRC) $(NODE_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $(CORE_SRC) $(NODE_SRC) $(LIBS) -lpthread

$(BUILD)/test_%: tests/test_%.c $(CORE_SRC) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $< $(CORE_SRC) $(LIBS) -lpthread

$(BUILD)/test_cli_status: tests/test_cli_status.c $(CORE_SRC) src/rps_cli.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $< $(CORE_SRC) src/rps_cli.c $(LIBS) -lpthread

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo $$t; $$t; done

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
