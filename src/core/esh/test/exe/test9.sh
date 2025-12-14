#!/bin/sh

k=0
while [ $k -ne 4 ] ; do
    echo 'k='
    echo $k
    k=`expr $k + 1`

    l=0
    while [ $l -ne 4 ]; do
        echo 'l='
        echo $l
        l=`expr $l + 1`

        if [ $l -eq 2 ]; then
            echo 'l='
            echo $l
            echo 'break while loop (l)'
            break
        fi
    done
done
