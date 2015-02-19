CCFlags=-std=gnu99 -Wall -Wpedantic -Wextra -I../common/ `pcre-config --cflags`
#CCFlags=-std=gnu99 -Wall -Wpedantic -Wextra -I../common/ `pcre-config --cflags` -g
#CCFlags=-std=gnu99 -Wall -Wpedantic -Wextra -I../common/ `pcre-config --cflags` -g -DDEBUG=1 
#CCFlags=-std=gnu99 -Wall -Wpedantic -Wextra -I../common/ `pcre-config --cflags` -g -DDEBUG=1 -DMOREDEBUG=1
LinkFlags=`pcre-config --libs`
#LinkFlags=`pcre-config --libs` -pg
CSV_GREP_FILES = csvgrep.o ../common/csv_tokenizer.o

csvgrep: $(CSV_GREP_FILES)
	$(CC) -o $@ $(LinkFlags) $(CSV_GREP_FILES) 

%.o: %.c
	$(CC) -c -o $@ $(CCFlags) $< 

clean:
	rm -f $(CSV_GREP_FILES) csvgrep
