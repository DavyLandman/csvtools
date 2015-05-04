#!/bin/bash
PROGRAM=$1
LARGE_FILES=$2
RESULT=0

test_normal() {
	REF=$(cat "$4")
	OUTPUT=$(../bin/$2 $3 < "$1")
	if (($? > 0)); then
		printf "\t- $1 params: \"$3\" =\t Failed ($2 crashed)\n"
		RESULT=1
		return
	fi


	if [ "$OUTPUT" != "$REF" ]; then
		printf "\t- $1 params: \"$3\" =\t Failed\n"
		printf "$OUTPUT" > /tmp/error-output.csv
		diff -a -d "$4" /tmp/error-output.csv
		rm /tmp/error-output.csv
		printf ""
		RESULT=1
	else
		printf  "\t- $1 params: \"$3\" =\t OK\n"
	fi
}

test_xz() {
	REF=$(xzcat "$4" | md5sum)
	OUTPUT=$(xzcat "$1" | ../bin/$2 $3 | md5sum)
	if (($? > 0)); then
		printf "\t- $1 params: \"$3\" =\t Failed ($2 crashed)\n"
		RESULT=1
		return
	fi


	if [ "$OUTPUT" != "$REF" ]; then
		printf "\t- $1 params: \"$3\" =\t Failed\n"
		RESULT=1
	else
		printf "\t- $1 params: \"$3\" =\t OK\n"
	fi
}
printf "Testing $PROGRAM"
for INPUT in $PROGRAM/*_input.csv*;
do
	ARGS=$(cat "$(printf $INPUT | sed 's/input\.csv.*$/command/')")
	OUTPUT=$(printf $INPUT | sed 's/input/output/')
	case $INPUT in
		*.csv.xz )
			if (($LARGE_FILES == 1)); then
				test_xz "$INPUT" "$PROGRAM" "$ARGS" "$OUTPUT"
			fi
		;;
		*.csv )
			test_normal "$INPUT" "$PROGRAM" "$ARGS" "$OUTPUT"
		;;
	esac
done
if [ $RESULT == 0 ]; then
	printf "Tests succeeded\n"
else
	printf "Tests failed\n"
fi
exit $RESULT
