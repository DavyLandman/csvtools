BUFFER_SIZE=1048576 # 1024K can be overridden with make BUFFER_SIZE=20
LinkFlags=
CFLAGS+=-std=gnu99 -Wall -pedantic -Wextra -DBUFFER_SIZE=$(BUFFER_SIZE) -fno-strict-aliasing

DISABLE_ASSERTS=-DNDEBUG
ifdef DEBUG # set with `make .. DEBUG=1`
CFLAGS+=-g -DDEBUG
ifdef VERBOSE
CFLAGS+=-DMOREDEBUG
endif
else
CFLAGS+=-O3 $(DISABLE_ASSERTS)
endif
ifdef PERF
CFLAGS+=-lprofiler -g
endif

DO_COVERAGE=""
ifdef COVERAGE
CFLAGS+=-coverage
DO_COVERAGE="COVERAGE=1"
endif


ifndef TEST_SLOW_PATH
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		CFLAGS += -D_GNU_SOURCE
	endif
else
	CFLAGS += -D_SLOW_PATH
endif

CSV_GREP_FILES = src/csvgrep.c src/csv_tokenizer.c
CSV_CUT_FILES = src/csvcut.c src/csv_tokenizer.c
CSV_TOK_TEST_COUNT_FILES = test/csv_tokenizer_counts.c src/csv_tokenizer.c
CSV_PIPE_FILES = src/csvpipe.c
CSV_UNPIPE_FILES = src/csvunpipe.c
CSV_AWK_FILES = src/csvawk.c
BENCH_FILES = bench/runner.c bench/generate.c bench/deps/pcg-c-basic/pcg_basic.c

.PHONY: all test clean test-csvgrep test-csvcut test-csvpipe test-csvunpipe test-all-sizes test-tokenizer install

all: bin/csvcut bin/csvgrep bin/csvpipe bin/csvunpipe bin/csvawk bin/csvawk

# yes, we recompile csv_tokenizer, it keeps the makefile simpler and it allows
# the compiler to do some cross module optimizations :)

bench: bin/bench
bin/bench: $(BENCH_FILES) bin/ all
	$(CC) -o $@ $(LinkFlags) $(CFLAGS) $(BENCH_FILES) 

bench/deps/pcg-c-basic/pcg_basic.c:
	(cd bench/deps/pcg-c-basic/ && git submodule init && git submodule update)
	(cd bench/deps/awk-csv-parser/ && git submodule init && git submodule update)

csvcut: bin/csvcut
bin/csvcut: $(CSV_CUT_FILES) Makefile bin/
	$(CC) -o $@ $(LinkFlags) $(CFLAGS) $(CSV_CUT_FILES) 

csvpipe: bin/csvpipe
bin/csvpipe: $(CSV_PIPE_FILES) Makefile bin/
	$(CC) -o $@ $(LinkFlags) $(CFLAGS) $(CSV_PIPE_FILES) 

csvunpipe: bin/csvunpipe
bin/csvunpipe: $(CSV_UNPIPE_FILES) Makefile bin/
	$(CC) -o $@ $(LinkFlags) $(CFLAGS) $(CSV_UNPIPE_FILES) 

csvawk: bin/csvawk
bin/csvawk: $(CSV_AWK_FILES) Makefile bin/
	$(CC) -o $@ $(LinkFlags) $(CFLAGS) $(CSV_AWK_FILES) 

csvgrep: bin/csvgrep
bin/csvgrep: $(CSV_GREP_FILES) Makefile bin/
	$(CC) -o $@ $(LinkFlags) `pcre-config --libs` $(CFLAGS) `pcre-config --cflags` $(CSV_GREP_FILES) 

bin/csvtokenizercounts: $(CSV_TOK_TEST_COUNT_FILES) Makefile bin/
	$(CC) -o $@ $(LinkFlags) $(CFLAGS) $(CSV_TOK_TEST_COUNT_FILES)

bin/:
	mkdir bin/

ifdef SKIP_LARGE_FILES
LARGE_FILES=0
else
LARGE_FILES=1
endif

test: test-csvgrep test-csvcut test-csvpipe test-csvunpipe test-csvawk test-tokenizer

test-csvgrep: bin/csvgrep
	cd test && ./runtest.sh csvgrep $(LARGE_FILES) $(DO_COVERAGE)

test-csvcut: bin/csvcut
	cd test && ./runtest.sh csvcut $(LARGE_FILES) $(DO_COVERAGE)
	
test-csvpipe: bin/csvpipe
	cd test && ./runtest.sh csvpipe $(LARGE_FILES) $(DO_COVERAGE)

test-csvunpipe: bin/csvunpipe
	cd test && ./runtest.sh csvunpipe $(LARGE_FILES) $(DO_COVERAGE)

test-csvawk: bin/csvawk
	cd test && ./runtest.sh csvawk $(LARGE_FILES) $(DO_COVERAGE)

test-tokenizer: bin/csvtokenizercounts
	cd test && ./runtest.sh csvtokenizercounts $(LARGE_FILES) $(DO_COVERAGE)

test-all-sizes: 
	 ./test/test-sizes.sh $(DO_COVERAGE)

test-all-sizes-ci: 
	 curl -s https://codecov.io/bash > /tmp/codecov.sh
	 bash /tmp/codecov.sh -x "llvm-cov gcov"
	 ./test/test-sizes.sh $(DO_COVERAGE)
	 bash /tmp/codecov.sh -x "llvm-cov gcov"
	 ./test/test-sizes.sh $(DO_COVERAGE) TEST_SLOW_PATH=1
	 bash /tmp/codecov.sh -x "llvm-cov gcov"



prefix=/usr/local
    
install: all
	install -m 0755 bin/csvcut $(prefix)/bin/csvcut
	install -m 0755 bin/csvgrep $(prefix)/bin/csvgrep
	install -m 0755 bin/csvawk $(prefix)/bin/csvawk
	install -m 0755 bin/csvpipe $(prefix)/bin/csvpipe
	install -m 0755 bin/csvunpipe $(prefix)/bin/csvunpipe

clean:
	rm -rf bin/*
deep-clean:
	rm -rf bin/*
	rm -rf *.gc{ov,da,no}
