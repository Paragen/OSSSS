#!/bin/bash

if  [ $# -ne 1 ]
then
	echo "incorrect arguments count $#"
	exit 1
fi
older=$(date +"%s")
let "older -= 24*3600*7"
bad_links() {
	for i in $1/*
	do
		if [ -h $i -a ! -e $i ] && [ $(stat -c "%Y" $i) -lt $older ]
		then
			echo $i
		fi
		if [ -d $i ]
		then
			bad_links $i
		fi
	done
}
bad_links $1
exit 0
