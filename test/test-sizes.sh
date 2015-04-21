#!/bin/sh


set -e

cd ..

#silent function from https://serverfault.com/questions/607884
SILENT_LOG=/tmp/silent_log_$$.txt
trap "/bin/rm -f $SILENT_LOG" EXIT

function report_and_exit {
    cat "${SILENT_LOG}"
    exit 1
}

function silent {
	`rm -f ${SILENT_LOG}`
    $* 2>>"${SILENT_LOG}" >> "${SILENT_LOG}" || report_and_exit;
}

function test_with_size() {
	if (($1 > 30)); then
		if (($1 > 72)) ; then # csvcut has to read the full header
			if (($1 > 145)); then # csvgrep has to fit the max line length in 2*BUFFER_SIZE
				make test-csvcut test-csvgrep BUFFER_SIZE=$1 DISABLE_ASSERTS=-g
			else
				make test-csvgrep BUFFER_SIZE=$1 DISABLE_ASSERTS=-g SKIP_LARGE_FILES=1
				if (($? > 0)); then
    				echo "\033[91mFailure with size $1\033[39m"
					return 1
				fi
				make test-csvcut BUFFER_SIZE=$1 DISABLE_ASSERTS=-g
			fi
		else
			make test-csvcut test-csvgrep BUFFER_SIZE=$1 DISABLE_ASSERTS=-g SKIP_LARGE_FILES=1
		fi
	fi
	if (($? > 0)); then
    	echo "\033[91mFailure with size $1\033[39m"
		return 1
	fi
	make test-csvpipe test-csvunpipe test-csvpipe test-csvunpipe test-csvawkpipe BUFFER_SIZE=$1 DISABLE_ASSERTS=-g
	if (($? > 0)); then
    	echo "\033[91mFailure with size $1\033[39m"
		return 1
	fi
	return 0
}

echo "Testing predefined sizes"
for s in 1 2 3 4 5 6 7 8 11 16 21 24 32 36 63 128 1024;
	do
		silent "make clean"
		echo "Testing size: \t $s"
		silent test_with_size $s
	done

echo "Trying 40 random sizes"
for x in {1..40};
	do
		silent "make clean"
		s=$(( ( RANDOM % 400 )  + 1 ));
		echo "Testing size: \t $s"
		silent test_with_size $s
	done
