#!/bin/sh

rm -rf *._ret

for v in `awk '{print $1}' duplicate.sorted | uniq`
do
    ./client -r query -a "http://localhost:8964/" -n "$i" -d "$1/$v" | grep name | sed -e "s/[\"|\,]//g" | awk -v n=$v '{if(n != $3) print n,$3;}'  | sort > $v._ret &

    p=`ps aux|grep client|wc -l`

    while [ $p -gt 5 ]
    do
        p=`ps aux|grep client|wc -l`
        sleep 1
    done
    
done

    p=`ps aux|grep client|wc -l`
    while [ $p -gt 1 ]
    do
        p=`ps aux|grep client|wc -l`
        sleep 1
    done
 
cat *._ret | sort > query_result
rm -rf *._ret
echo "DONE!\n"

