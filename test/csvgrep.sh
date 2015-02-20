#! /bin/bash

RESULT=0

function test_csvgrep() {
	REF=`cat data/$1 | csvgrep $3`
	if (($? > 0)); then
		echo "Test failed: " $1 " : " $2 " (csvgrep failed)" 
		RESULT=1
		return
	fi

	OUTPUT=`cat data/$1 | ../bin/csvgrep $2`
	if (($? > 0)); then
		echo "Test failed: " $1 " : " $2 " (./csvgrep failed)" 
		RESULT=1
		return
	fi


	if [ "$OUTPUT" != "$REF" ]; then
		echo "Test failed: " $1 " : " $2
		echo $REF
		echo $OUTPUT
		RESULT=1
	fi
}

function test_csvgrep_xz() {
	REF=`xzcat data/$1 | csvgrep $3 | md5sum`
	if (($? > 0)); then
		echo "Test failed: " $1 " : " $2 " (csvgrep failed)" 
		RESULT=1
		return
	fi
	OUTPUT=`xzcat data/$1 | ../bin/csvgrep $2 | md5sum`
	if (($? > 0)); then
		echo "Test failed: " $1 " : " $2 " (./csvgrep failed)" 
		RESULT=1
		return
	fi
	if [ "$OUTPUT" != "$REF" ]; then
		echo "Test failed: " $1 " : " $2
		RESULT=1
	fi
}

test_csvgrep "simple.csv" "-p a/[0-9]+/" "-c a -r [0-9]+" 
test_csvgrep "simple.csv" "-p a/[1-2]/ -p b/[2-3]/" "-c a,b -r [1-3]" 
test_csvgrep "simple.csv" "-v -p a/[1-2]/ -p b/[2-3]/" "-i -c a,b -r [1-3]" 
test_csvgrep "corners.csv" "-p a/^$/" "-c a -r ^$" 
test_csvgrep "corners.csv" "-p a/[\"]/" "-c a -r [\"]"
test_csvgrep "corners.csv" "-v -p a/[\"]/" "-i -c a -r [\"]"
test_csvgrep "large-fields.csv" "-p column1/(foo|bar)/" "-c column1 -r .*(foo|bar).*" 
test_csvgrep "large-fields.csv" "-p column3/(foo|bar)/" "-c column3 -r .*(foo|bar).*" 
test_csvgrep "large-fields.csv" "-v -p column3/(foo|bar)/" "-i -c column3 -r .*(foo|bar).*" 

test_csvgrep_xz "canada-2011-census.csv.xz" "-p Characteristic/201[0-2]/" "-c Characteristic -r .*201[0-2].*" 
test_csvgrep_xz "canada-2011-census.csv.xz" "-p Topic/[A-Z][a-e]/" "-c Topic -r .*[A-Z][a-e].*" 

if [ $RESULT == 0 ]; then
	echo "Tests succeeded"
else
	echo "Tests failed"
fi
exit $RESULT
