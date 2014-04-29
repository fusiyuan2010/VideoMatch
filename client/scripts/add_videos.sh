#!/bin/sh


for i in `ls $1`
do
    ./client -r add -a "http://localhost:8964/" -n "$i" -d "$1/$i" &
    p=`ps aux|grep client|wc -l`

    while [ $p -gt 5 ]
    do
        p=`ps aux|grep client|wc -l`
        sleep 1
    done

done

