#!/bin/sh

k=0
while [ $k -ne 3 ]; do
	k=`expr $k + 1`
	echo 'outer'
	echo "k=$k"

	l=0
	while [ $l -ne 3 ]; do
		l=`expr $l + 1`
		echo 'inner'
		echo "l=$l"

		if [ $l -eq 2 ]; then
			echo 'break 2'
			break $l
		fi
	done

	echo 'not reached'
done

echo 'after loops'
