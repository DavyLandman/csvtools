#!/bin/bash
PROGRAM=$1
LARGE_FILES=$2
RESULT=0

DO_COVERAGE=false
if [ "$#" -eq 2 ]; then
    DO_COVERAGE=true
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

test_normal() {
    REF_FILE=$OUTPUT
	REF=$(cat "$OUTPUT")
    OUTPUT=$("../bin/$PROGRAM" "${ARGS[@]}" < "$INPUT")
	if (($? > 0)); then
        printf "\t- %s params: \"%s\" = \t Failed (%s crashed)\n" "$INPUT" "${ARGS[*]}" "$PROGRAM"
		RESULT=1
		return
	fi


	if [ "$OUTPUT" != "$REF" ]; then
        printf "\t- %s params: \"%s\" = \t Failed\n" "$INPUT" "${ARGS[*]}"
		printf "$OUTPUT" > /tmp/error-output.csv
        diff -a -d "$REF_FILE" /tmp/error-output.csv
		rm /tmp/error-output.csv
		printf ""
		RESULT=1
	else
        printf "\t- %s params: \"%s\" = \t OK\n" "$INPUT" "${ARGS[*]}" 
	fi
}

test_xz() {
	REF=$(xzcat "$OUTPUT" | md5sum)
	OUTPUT=$(xzcat "$INPUT" | "../bin/$PROGRAM" "${ARGS[@]}" | md5sum)
	if (($? > 0)); then
        printf "\t- %s params: \"%s\" = \t Failed (%s crashed)\n" "$INPUT" "${ARGS[*]}" "$PROGRAM"
		RESULT=1
		return
	fi


	if [ "$OUTPUT" != "$REF" ]; then
        printf "\t- %s params: \"%s\" = \t Failed\n" "$INPUT" "${ARGS[*]}"
		RESULT=1
	else
        printf "\t- %s params: \"%s\" = \t OK\n" "$INPUT" "${ARGS[*]}" 
	fi
}
printf "Testing $PROGRAM"
OUTPUT=$("../bin/$PROGRAM" -h 2>&1 | wc -l)
if (($? > 0)) || [ $OUTPUT -lt 1 ]; then
    printf "\t- %s has no help params" "$PROGRAM"
    RESULT=1
fi

for INPUT in $PROGRAM/*_input.csv*;
do
    source "$(printf $INPUT | sed 's/input\.csv.*$/command/')"
	#ARGS=$(cat "$(printf $INPUT | sed 's/input\.csv.*$/command/')")
	OUTPUT=$(printf $INPUT | sed 's/input/output/')
	case $INPUT in
		*.csv.xz )
			if (($LARGE_FILES == 1)); then
				test_xz
			fi
		;;
		*.csv )
			test_normal
		;;
	esac
done
if [ $RESULT == 0 ]; then
	printf "Tests succeeded\n"
else
	printf "Tests failed\n"
fi
report_coverage
exit $RESULT
