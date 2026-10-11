#!/bin/sh

k=0
while [ $k -ne 3 ]; do
	k=`expr $k + 1`
	echo 'outer'
	echo $k

	l=0
	while [ $l -ne 3 ]; do
		l=`expr $l + 1`
		echo 'inner'
		echo $l

		if [ $l -eq 2 ]; then
			echo 'break 2'
			break 2
		fi
	done

	echo 'not reached'
done

echo 'after loops'
