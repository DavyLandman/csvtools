#!/bin/sh

cd ..

function test_with_size() {
	if (($1 > 30)); then
		if (($1 > 72)) ; then # csvcut has to read the full header
			if (($1 > 200)); then # csvgrep has to fit the max line length in 2*BUFFER_SIZE
				make test-csvcut test-csvgrep BUFFER_SIZE=$1 DISABLE_ASSERTS=-g
			else
				make test-csvgrep BUFFER_SIZE=$1 DISABLE_ASSERTS=-g SKIP_LARGE_FILES=1
				if (($? > 0)); then
					echo "Failure with size $1"
					exit 1
				fi
				make test-csvcut BUFFER_SIZE=$1 DISABLE_ASSERTS=-g
			fi
		else
			make test-csvcut test-csvgrep BUFFER_SIZE=$1 DISABLE_ASSERTS=-g SKIP_LARGE_FILES=1
		fi
	fi
	if (($? > 0)); then
		echo "Failure with size $1"
		exit 1
	fi
	make test-csvpipe test-csvunpipe BUFFER_SIZE=$1 DISABLE_ASSERTS=-g
	if (($? > 0)); then
		echo "Failure with size $1"
		exit 1
	fi
}

echo "Testing predefined sizes"
for s in 1 3 5 8 16 20 32 128 1024;
	do
		`make clean`
		test_with_size $s
	done

echo "Trying random sizes"
for x in {1..40};
	do
		`make clean`
		test_with_size $(( ( RANDOM % 400 )  + 1 ))
	done
