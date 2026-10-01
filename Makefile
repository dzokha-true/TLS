CC ?= cc
AR ?= ar
CPPFLAGS += -D_DEFAULT_SOURCE -I.
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Werror -pthread
LDLIBS += -pthread
BUILD ?= build

.PHONY: all test sanitize thread-sanitize clean
all: $(BUILD)/libtls.a

$(BUILD):
	mkdir -p $@

$(BUILD)/TLS.o: TLS.c TLS.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c TLS.c -o $@

$(BUILD)/libtls.a: $(BUILD)/TLS.o
	$(AR) rcs $@ $^

$(BUILD)/test-tls: test/test-tls.c $(BUILD)/libtls.a TLS.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(BUILD)/libtls.a $(LDLIBS) -o $@

$(BUILD)/test-internals: test/test-internals.c TLS.c TLS.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@

test: $(BUILD)/test-tls $(BUILD)/test-internals
	$(BUILD)/test-tls
	$(BUILD)/test-internals

sanitize:
	$(MAKE) test BUILD=build/asan CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined'

thread-sanitize:
	$(MAKE) test BUILD=build/tsan CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=thread -fno-omit-frame-pointer' LDFLAGS='-fsanitize=thread'

clean:
	rm -rf build
