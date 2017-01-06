INCLUDES := -I../include -I../../
CFLAGS := $(CFLAGS) -g -O2 -Wall -D_GNU_SOURCE -pthread $(INCLUDES)
LDFLAGS := $(LDFLAGS) -pthread -lrt

HEADERS := \
	../include/futextest.h \
	../include/atomic.h \
	../include/logging.h
TEST_TARGETS := \
	futex_wait_timeout \
	futex_wait_wouldblock \
	futex_requeue_pi \
	futex_requeue_pi_signal_restart \
	futex_requeue_pi_mismatched_ops \
	futex_wait_uninitialized_heap \
	futex_wait_private_mapped_file

TEST_PROGS := $(TEST_TARGETS) run.sh

.PHONY: all clean
all: $(TEST_TARGETS)

$(TEST_TARGETS): $(HEADERS)

include ../../lib.mk

clean:
	rm -f $(TEST_TARGETS)
