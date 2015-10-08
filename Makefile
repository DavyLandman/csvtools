BUFFER_SIZE=1048576 # 1024K can be overridden with make BUFFER_SIZE=20
LinkFlags=
CFLAGS+=-std=gnu99 -Wall -pedantic -Wextra -DBUFFER_SIZE=$(BUFFER_SIZE) -fno-strict-aliasing

DISABLE_ASSERTS=-DNDEBUG=1
ifdef DEBUG # set with `make .. DEBUG=1`
CFLAGS+=-g -DDEBUG=1
ifdef VERBOSE
CFLAGS+=-DMOREDEBUG=1
endif
else
CFLAGS+=-O3 $(DISABLE_ASSERTS)
endif
ifdef PERF
CFLAGS+=-ggdb
endif

CSV_GREP_FILES = src/csvgrep.c src/csv_tokenizer.c
CSV_CUT_FILES = src/csvcut.c src/csv_tokenizer.c
CSV_PIPE_FILES = src/csvpipe.c
CSV_UNPIPE_FILES = src/csvunpipe.c
CSV_AWKPIPE_FILES = src/csvawkpipe.c

.PHONY: all test clean test-csvgrep test-csvcut test-csvpipe test-csvunpipe test-all-sizes install

all: bin/csvcut bin/csvgrep bin/csvpipe bin/csvunpipe bin/csvawkpipe bin/csvawk

# yes, we recompile csv_tokenizer, it keeps the makefile simpler and it allows
# the compiler to do some cross module optimizations :)

csvcut: bin/csvcut
bin/csvcut: $(CSV_CUT_FILES) Makefile bin/
	$(CC) -o $@ $(LinkFlags) $(CFLAGS) $(CSV_CUT_FILES) 

csvpipe: bin/csvpipe
bin/csvpipe: $(CSV_PIPE_FILES) Makefile bin/
	$(CC) -o $@ $(LinkFlags) $(CFLAGS) $(CSV_PIPE_FILES) 

csvunpipe: bin/csvunpipe
bin/csvunpipe: $(CSV_UNPIPE_FILES) Makefile bin/
	$(CC) -o $@ $(LinkFlags) $(CFLAGS) $(CSV_UNPIPE_FILES) 

csvawkpipe: bin/csvawkpipe
bin/csvawkpipe: $(CSV_AWKPIPE_FILES) Makefile bin/
	$(CC) -o $@ $(LinkFlags) $(CFLAGS) $(CSV_AWKPIPE_FILES) 

csvgrep: bin/csvgrep
bin/csvgrep: $(CSV_GREP_FILES) Makefile bin/
	$(CC) -o $@ $(LinkFlags) `pcre-config --libs` $(CFLAGS) `pcre-config --cflags` $(CSV_GREP_FILES) 

csvawk: bin/csvawk
bin/csvawk: src/csvawk.sh bin/csvawkpipe bin/
	cp src/csvawk.sh bin/csvawk

bin/:
	mkdir bin/

ifdef SKIP_LARGE_FILES
LARGE_FILES=0
else
LARGE_FILES=1
endif

test: test-csvgrep test-csvcut test-csvpipe test-csvunpipe test-csvawkpipe

test-csvgrep: bin/csvgrep
	cd test && ./runtest.sh csvgrep $(LARGE_FILES)

test-csvcut: bin/csvcut
	cd test && ./runtest.sh csvcut $(LARGE_FILES)
	
test-csvpipe: bin/csvpipe
	cd test && ./runtest.sh csvpipe $(LARGE_FILES)

test-csvunpipe: bin/csvunpipe
	cd test && ./runtest.sh csvunpipe $(LARGE_FILES)

test-csvawkpipe: bin/csvawkpipe
	cd test && ./runtest.sh csvawkpipe $(LARGE_FILES)

test-all-sizes: 
	cd test && ./test-sizes.sh

prefix=/usr/local
    
install: all
	install -m 0755 bin/csvcut $(prefix)/bin/csvcut
	install -m 0755 bin/csvgrep $(prefix)/bin/csvgrep
	install -m 0755 bin/csvawk $(prefix)/bin/csvawk
	install -m 0755 bin/csvawkpipe $(prefix)/bin/csvawkpipe
	install -m 0755 bin/csvpipe $(prefix)/bin/csvpipe
	install -m 0755 bin/csvunpipe $(prefix)/bin/csvunpipe

clean:
	rm -rf bin/*
