#!/bin/sh

bin_dir=$(dirname `readlink -f $0`)

cd $bin_dir

$bin_dir/message.sh > /dev/null 2> /dev/null &
#$bin_dir/message.sh &

sleep 1

nodeid=2000
nickname=mira
dataport=9000
sitcp_ip=192.168.1.7
module_num=0

$bin_dir/frontend.sh \
    $nickname \
    $nodeid \
    $dataport \
    $sitcp_ip \
    $module_num \
#    >/dev/null 2>/dev/null &
