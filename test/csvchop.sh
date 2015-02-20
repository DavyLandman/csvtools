#! /bin/bash

RESULT=0

function test_csvchop() {
	REF=`cat data/$1 | csvcut $3`
	if (($? > 0)); then
		echo "Test failed: " $1 " : " $2 " (csvcut failed)" 
		RESULT=1
		return
	fi

	OUTPUT=`cat data/$1 | ../bin/csvchop $2`
	if (($? > 0)); then
		echo "Test failed: " $1 " : " $2 " (csvchop failed)" 
		RESULT=1
		return
	fi


	if [ "$OUTPUT" != "$REF" ]; then
		echo "Test failed: " $1 " : " $2
		RESULT=1
	fi
}

function test_csvchop_xz() {
	REF=`xzcat data/$1 | csvcut $3 | md5sum`
	if (($? > 0)); then
		echo "Test failed: " $1 " : " $2 " (csvcut failed)" 
		RESULT=1
		return
	fi
	OUTPUT=`xzcat data/$1 | ../bin/csvchop $2 | md5sum`
	if (($? > 0)); then
		echo "Test failed: " $1 " : " $2 " (csvchop failed)" 
		RESULT=1
		return
	fi
	if [ "$OUTPUT" != "$REF" ]; then
		echo "Test failed: " $1 " : " $2
		RESULT=1
	fi
}

test_csvchop "simple.csv" "-d a" "-C a" 
test_csvchop "simple.csv" "-d a,b" "-C a,b" 
test_csvchop "simple.csv" "-k a,b" "-c a,b" 
test_csvchop "simple.csv" "-k a,e" "-c a,e" 
test_csvchop "corners.csv" "-k a,b" "-c a,b" 
test_csvchop "corners.csv" "-d a,b" "-C a,b" 
test_csvchop "large-fields.csv" "-k column1" "-c column1" 
test_csvchop "large-fields.csv" "-k column2,column3" "-c column2,column3" 
test_csvchop "large-fields.csv" "-K 1,2" "-c column2,column3" 

test_csvchop_xz "canada-2011-census.csv.xz" "-d Note" "-C Note" 

if [ $RESULT == 0 ]; then
	echo "Tests succeeded"
else
	echo "Tests failed"
fi
exit $RESULT
