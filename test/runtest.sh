#!/bin/sh
PROGRAM=$1
LARGE_FILES=$2
RESULT=0

test_normal() {
	REF=`cat $4`
	OUTPUT=`cat $1 | ../bin/$2 $3`
	if (($? > 0)); then
		echo "\t- $1 params: \"$3\" =\t Failed ($2 crashed)"
		RESULT=1
		return
	fi


	if [ "$OUTPUT" != "$REF" ]; then
		echo "\t- $1 params: \"$3\" =\t Failed"
		echo "$OUTPUT" > /tmp/error-output.csv
		diff -a -d "$4" /tmp/error-output.csv
		rm /tmp/error-output.csv
		echo ""
		RESULT=1
	else
		echo  "\t- $1 params: \"$3\" =\t OK"
	fi
}

test_xz() {
	REF=`xzcat $4 | md5sum`
	OUTPUT=`xzcat $1 | ../bin/$2 $3 | md5sum`
	if (($? > 0)); then
		echo "\t- $1 params: \"$3\" =\t Failed ($2 crashed)"
		RESULT=1
		return
	fi


	if [ "$OUTPUT" != "$REF" ]; then
		echo "\t- $1 params: \"$3\" =\t Failed"
		RESULT=1
	else
		echo "\t- $1 params: \"$3\" =\t OK"
	fi
}
echo "Testing $PROGRAM"
for INPUT in $PROGRAM/*_input.csv*;
do
	ARGS=`cat $(echo $INPUT | sed 's/input\.csv.*$/command/')`
	OUTPUT=`echo $INPUT | sed 's/input/output/'`
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
	echo "Tests succeeded"
else
	echo "Tests failed"
fi
exit $RESULT
