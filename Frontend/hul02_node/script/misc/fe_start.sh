#!/bin/sh

bin_dir=$(dirname `readlink -f $0`)

cd $bin_dir

#$bin_dir/message.sh & #> /dev/null 2> /dev/null &
./message.sh > /dev/null 2> /dev/null &

sleep 1

for i in $(seq 1 1)
do
    nodeid=`expr $((0x630)) + $i`
    nickname=hul02misc-`expr + $i`
    dataport=`expr 9020 + $i` 
    sitcp_ip=192.168.11.`expr 13 + $i`
    min_window=0
#    max_window=188
    # max_window=225
    max_window=600
    only_leading=1
    master=--slave

    $bin_dir/frontend.sh \
	$nickname \
	$nodeid \
	$dataport \
	$sitcp_ip \
	$min_window \
	$max_window \
	$only_leading \
	$master \
	>/dev/null 2>/dev/null &
done

#    nodeid=0x630
#    nickname=hul02misc
#    dataport=9021 
#    sitcp_ip=192.168.11.14
#    min_window=37
#    max_window=188
#    master=--master
#
#    $bin_dir/frontend.sh \
#	$nickname \
#	$nodeid \
#	$dataport \
#	$sitcp_ip \
#	$min_window \
#	$max_window \
#	$master \
#	>/dev/null 2>/dev/null &
