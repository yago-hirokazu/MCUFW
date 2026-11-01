#!/bin/sh

k='close quote 1'
echo $k

k='close quote 2'
echo $k

k='close quote 3'
if [ $k = 'close quote 3' ]; then
   echo $k
fi

k='close quote 4'
if [ $k = 'close quote 4' ]
then
   echo $k
fi


