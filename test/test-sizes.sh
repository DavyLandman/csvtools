#!/bin/sh
# run from root dir!


EXTRA_FLAGS=""
DO_COVERAGE=false
if [ "$#" -eq 1 ]; then
    DO_COVERAGE=true
    EXTRA_FLAGS="COVERAGE=1"
    echo "Downloading codecov code"
    curl -s https://codecov.io/bash > /tmp/codecov.sh
fi

RUN=1

report_coverage() {
    if [ "$DO_COVERAGE" = true ]; then
        echo "Reporting to codecov"
        bash /tmp/codecov.sh -b "sizes-run-$RUN" 
        RUN=$((RUN+1))
    fi
}

set -e

#silent function from https://serverfault.com/questions/607884
SILENT_LOG=/tmp/silent_log_$$.txt
trap "/bin/rm -f $SILENT_LOG" EXIT

report_and_exit() {
    cat "${SILENT_LOG}"
    exit 1
}

silent() {
	`rm -f ${SILENT_LOG}`
    $* 2>>"${SILENT_LOG}" >> "${SILENT_LOG}" || report_and_exit;
}

test_with_size() {
	if (($1 > 30)); then
		if (($1 > 72)) ; then # csvcut has to read the full header
			if (($1 > 145)); then # csvgrep has to fit the max line length in 2*BUFFER_SIZE
				make test-csvcut test-csvgrep BUFFER_SIZE=$1 DISABLE_ASSERTS=-g $EXTRA_FLAGS
			else
				make test-csvgrep BUFFER_SIZE=$1 DISABLE_ASSERTS=-g SKIP_LARGE_FILES=1 $EXTRA_FLAGS
				if (($? > 0)); then
    				echo "\033[91mFailure with size $1\033[39m"
					return 1
				fi
				make test-csvcut BUFFER_SIZE=$1 DISABLE_ASSERTS=-g $EXTRA_FLAGS
			fi
		else
			make test-csvcut test-csvgrep BUFFER_SIZE=$1 DISABLE_ASSERTS=-g SKIP_LARGE_FILES=1 $EXTRA_FLAGS
		fi
	fi
	if (($? > 0)); then
    	echo "\033[91mFailure with size $1\033[39m"
		return 1
	fi
	make test-csvpipe test-csvunpipe test-csvpipe test-csvunpipe test-csvawkpipe BUFFER_SIZE=$1 DISABLE_ASSERTS=-g $EXTRA_FLAGS
	if (($? > 0)); then
    	echo "\033[91mFailure with size $1\033[39m"
		return 1
	fi
	if (($1 > 1)); then
        make test-tokenizer BUFFER_SIZE=$1 DISABLE_ASSERTS=-g $EXTRA_FLAGS
        if (($? > 0)); then
            echo "\033[91mFailure with size $1\033[39m"
            return 1
        fi
	fi
    report_coverage
	return 0
}

echo "Testing predefined sizes"
for s in 1 2 3 4 5 6 7 8 11 16 21 24 32 36 63 128 1024;
	do
		silent "make deep-clean"
		echo "Testing size: \t $s"
		silent test_with_size $s
	done

echo "Trying 40 random sizes"
for x in $(seq 1 40);
	do
		silent "make deep-clean"
        RANDOMNUM=$( head -200 /dev/urandom | cksum | cut -f1 -d " ")
		s=$(( ( RANDOMNUM % 400 )  + 1 ));
        echo "Testing size: \t $s (run $x/40)"
		silent test_with_size $s
	done
