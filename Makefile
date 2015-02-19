CCFlags=-std=gnu99 -Wall -Wpedantic -Wextra `pcre-config --cflags`
LinkFlags=
CSV_GREP_FILES = obj/csvgrep.o obj/csv_tokenizer.o
CSV_CHOP_FILES = obj/csvchop.o obj/csv_tokenizer.o obj/string_utils.o

all: bin/csvchop bin/csvgrep

bin/csvchop: $(CSV_CHOP_FILES)
	$(CC) -o $@ $(LinkFlags) $(CSV_CHOP_FILES) 

bin/csvgrep: $(CSV_GREP_FILES)
	$(CC) -o $@ $(LinkFlags) `pcre-config --libs` $(CSV_GREP_FILES) 

obj/%.o: src/%.c
	$(CC) -c -o $@ $(CCFlags) $< 

clean:
	rm -f bin/csv*
	rm -f obj/*.o
