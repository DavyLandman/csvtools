BUFFER_SIZE=1048576 # 1024K can be overridden with make BUFFER_SIZE=20
LinkFlags=
CCFlags=-std=gnu99 -Wall -Wpedantic -Wextra -DBUFFER_SIZE=$(BUFFER_SIZE)

DISABLE_ASSERTS=-DNDEBUG=1
ifdef DEBUG # set with `make .. DEBUG=1`
CCFlags+=-g -DDEBUG=1
ifdef VERBOSE
CCFlags+=-DMOREDEBUG=1
endif
else
CCFlags+=-O2 $(DISABLE_ASSERTS)
endif

CSV_GREP_FILES = src/csvgrep.c src/csv_tokenizer.c
CSV_CUT_FILES = src/csvcut.c src/csv_tokenizer.c
CSV_PIPE_FILES = src/csvpipe.c
CSV_UNPIPE_FILES = src/csvunpipe.c

.PHONY: all test clean test-csvgrep test-csvcut test-csvpipe test-csvunpipe test-all-sizes

all: bin/csvcut bin/csvgrep bin/csvpipe bin/csvunpipe

# yes, we recompile csv_tokenizer, it keeps the makefile simpler and it allows
# the compiler to do some cross module optimizations :)

bin/csvcut: $(CSV_CUT_FILES) Makefile
	$(CC) -o $@ $(LinkFlags) $(CCFlags) $(CSV_CUT_FILES) 

bin/csvpipe: $(CSV_PIPE_FILES) Makefile
	$(CC) -o $@ $(LinkFlags) $(CCFlags) $(CSV_PIPE_FILES) 

bin/csvunpipe: $(CSV_UNPIPE_FILES) Makefile
	$(CC) -o $@ $(LinkFlags) $(CCFlags) $(CSV_UNPIPE_FILES) 

bin/csvgrep: $(CSV_GREP_FILES) Makefile
	$(CC) -o $@ $(LinkFlags) `pcre-config --libs` $(CCFlags) `pcre-config --cflags` $(CSV_GREP_FILES) 

ifdef SKIP_LARGE_FILES
LARGE_FILES=0
else
LARGE_FILES=1
endif

test: test-csvgrep test-csvcut test-csvpipe test-csvunpipe

test-csvgrep: bin/csvgrep
	cd test && ./runtest.sh csvgrep $(LARGE_FILES)

test-csvcut: bin/csvcut
	cd test && ./runtest.sh csvcut $(LARGE_FILES)
	
test-csvpipe: bin/csvpipe
	cd test && ./runtest.sh csvpipe $(LARGE_FILES)

test-csvunpipe: bin/csvunpipe
	cd test && ./runtest.sh csvunpipe $(LARGE_FILES)

test-all-sizes: 
	cd test && ./test-sizes.sh

clean:
	rm -rf bin/*
