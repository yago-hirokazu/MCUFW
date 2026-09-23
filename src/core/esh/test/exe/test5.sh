#!/bin/sh

k=0
if [ $k -eq 0 ]; then
    echo 'catch at if'
    echo $k
elif [ $k -eq 1 ]; then
    echo 'catch at elif'
    echo $k
else
    echo 'catch at else'
    echo $k
fi

k=1
if [ $k -eq 0 ]; then
    echo 'catch at if'
    echo $k
elif [ $k -eq 1 ]; then
    echo 'catch at elif'
    echo $k
else
    echo 'catch at else'
    echo $k
fi

