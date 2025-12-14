#!/bin/sh

k=0
echo $k
while [ $k -ne 4 ]; do
    k=`expr $k + 1`
    echo $k

    if [ $k -le 2 ]; then
        echo 'k <= 2'
    else
        echo 'k > 2'
    fi
done
