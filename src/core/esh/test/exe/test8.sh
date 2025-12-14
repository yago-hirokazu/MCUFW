#!/bin/sh

k=0
while [ $k -ne 10 ] ; do
    echo $k
    k=`expr $k + 1`

    if [ $k -eq 7 ]; then
        echo 'break while loop'
        break
    fi
done
