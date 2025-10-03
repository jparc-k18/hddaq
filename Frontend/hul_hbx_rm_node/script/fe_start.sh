#!/bin/sh

bin_dir=$(dirname `readlink -f $0`)

cd $bin_dir

 $bin_dir/message.sh > /dev/null 2> /dev/null &

sleep 1

#for i in $(seq 0 1)
for i in $(seq 1 1)
do 
    nodeid=`expr 2000 + $i`
    nickname=hul_hbx_scr`expr 3 - $i`
    dataport=`expr 9000 + $i` 
    sitcp_ip=192.168.12.`expr 10 + $i`
    master=--master
    en_block=0x3

    $bin_dir/frontend.sh \
	$nickname \
	$nodeid \
	$dataport \
	$sitcp_ip \
	$master \
	$en_block \
	>/dev/null 2>/dev/null &
done
