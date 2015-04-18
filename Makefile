CCFlags=-std=gnu99 -Wall -Wpedantic -Wextra `pcre-config --cflags` -g 
LinkFlags=
CSV_GREP_FILES = bin/obj/csvgrep.o bin/obj/csv_tokenizer.o
CSV_CUT_FILES = bin/obj/csvcut.o bin/obj/csv_tokenizer.o
CSV_PIPE_FILES = bin/obj/csvpipe.o
CSV_UNPIPE_FILES = bin/obj/csvunpipe.o

.PHONY: test clean test-csvgrep test-csvcut

all: bin/csvcut bin/csvgrep bin/csvpipe bin/csvunpipe

bin/csvcut: $(CSV_CUT_FILES) Makefile
	$(CC) -o $@ $(LinkFlags) $(CSV_CUT_FILES) 

bin/csvpipe: $(CSV_PIPE_FILES) Makefile
	$(CC) -o $@ $(LinkFlags) $(CSV_PIPE_FILES) 

bin/csvunpipe: $(CSV_UNPIPE_FILES) Makefile
	$(CC) -o $@ $(LinkFlags) $(CSV_UNPIPE_FILES) 

bin/csvgrep: $(CSV_GREP_FILES) Makefile
	$(CC) -o $@ $(LinkFlags) `pcre-config --libs` $(CSV_GREP_FILES) 

bin/obj/%.o: src/%.c bin/obj/ Makefile
	$(CC) -c -o $@ $(CCFlags) $< 

bin/obj/: 
	mkdir -p bin/obj

test: test-csvgrep test-csvcut

test-csvgrep: bin/csvgrep
	cd test && ./runtest.sh csvgrep

test-csvcut: bin/csvcut
	cd test && ./csvcut.sh
	
test-csvpipe: bin/csvpipe
	cd test && ./runtest.sh csvpipe

test-csvunpipe: bin/csvunpipe
	cd test && ./runtest.sh csvunpipe

clean:
	rm -f bin/csv*
	rm -f bin/obj/*.o
