CCFlags=-std=gnu99 -Wall -Wpedantic -Wextra `pcre-config --cflags`
LinkFlags=
CSV_GREP_FILES = bin/obj/csvgrep.o bin/obj/csv_tokenizer.o
CSV_CHOP_FILES = bin/obj/csvchop.o bin/obj/csv_tokenizer.o bin/obj/string_utils.o

.PHONY: test clean

all: bin/csvchop bin/csvgrep

bin/csvchop: $(CSV_CHOP_FILES)
	$(CC) -o $@ $(LinkFlags) $(CSV_CHOP_FILES) 

bin/csvgrep: $(CSV_GREP_FILES)
	$(CC) -o $@ $(LinkFlags) `pcre-config --libs` $(CSV_GREP_FILES) 

bin/obj/%.o: src/%.c bin/obj/
	$(CC) -c -o $@ $(CCFlags) $< 

bin/obj/: bin/
	mkdir bin/obj

bin/:
	mkdir bin/

test:
	cd test && ./csvgrep.sh
	cd test && ./csvchop.sh

clean:
	rm -f bin/csv*
	rm -f bin/obj/*.o
