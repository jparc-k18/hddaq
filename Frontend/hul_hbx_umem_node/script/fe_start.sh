#!/bin/sh

bin_dir=$(dirname `readlink -f $0`)

cd $bin_dir

$bin_dir/message.sh > /dev/null 2> /dev/null &

sudo /home/axis/CCUSB_AD413A_Init/Init_AD413A

sleep 1

nodeid=2003
nickname=hul_hbx_umem
dataport=9003 
sitcp_ip=192.168.12.13
master=--slave

$bin_dir/frontend.sh \
    $nickname \
    $nodeid \
    $dataport \
    $sitcp_ip \
    $master \
    >/dev/null 2>/dev/null &
