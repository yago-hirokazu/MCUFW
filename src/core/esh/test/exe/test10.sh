#!/bin/sh

k=0
while [ $k -ne 4 ]; do
    k=`expr $k + 1`
    echo 'k='
    echo $k

    if [ $k -eq 2 ]; then
        l=0
        while [ $l -ne 4 ]; do
            echo 'l='
            echo $l
            l=`expr $l + 1`

            if [ $l -eq 3 ]; then
                echo 'break while loop (l)'
                break
            fi
        done
    else
        echo 'k > 2'
    fi
done
