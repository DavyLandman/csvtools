#!/bin/bash

set -o pipefail
set -o errtrace
set -o nounset
set -o errexit

 
function usage() { 
	echo "Usage: $0 [options] 'AWK script' [input file]" 1>&2; 	
	echo "-s ," 1>&2; 	
	echo "  Which character to use as an separator (default is ,)" 1>&2; 	
	echo "-d" 1>&2; 	
	echo "  Drop header row" 1>&2;
	exit 1; 
}

CSV_ARGS=''

while getopts ":ds:" opt; do
	case $opt in
	d)
		CSV_ARGS+=' -d'
		;;
	s)
		CSV_ARGS+=" -s $OPTARG"
		;;
	\?)
		echo "Invalid option: -$OPTARG" >&2
		usage
		exit 1
		;;
	:)
		echo "Option -$OPTARG requires an argument." >&2
		usage
		exit 1
		;;
	esac
done
shift $((OPTIND-1))

if [ $# -lt 1 ]; then
	echo "Forgot to pass the script to run" 1>&2 ;
	usage
	exit 1
fi

AWK_SCRIPT='BEGIN{ FS="\x1F"; RS="\x1E" } '
AWK_SCRIPT+=${1}

pushd `dirname $0` > /dev/null
SCRIPTPATH=`pwd`
popd > /dev/null

CSVAWKPIPE="$SCRIPTPATH/csvawkpipe"

if [ -f $CSVAWKPIPE ]; then
	if [ $# -eq 2  ]; then
		$CSVAWKPIPE $CSV_ARGS "$2" | awk "$AWK_SCRIPT"
	else
		$CSVAWKPIPE $CSV_ARGS | awk "$AWK_SCRIPT"
	fi
else
	echo "cannot find csvawkpipe (tried: $CSVAWKPIPE)" 2>1;
	exit 1;
fi
