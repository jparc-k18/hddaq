#!/bin/sh

bin_dir=$(dirname `readlink -f $0`)

cd $bin_dir

$bin_dir/message.sh > /dev/null 2> /dev/null &

sleep 1

reg_dir=${HOME}/vme-easiroc-registers
#reg_dir=default
adc=on
adc_off=off
tdc=on

#for i in $(seq 16 55)
#for i in $(seq 96 102)
#for i in $(seq 16 17) $(seq 19 31)
for i in $(seq 16 17) $(seq 19 31) $(seq 96 102)
do
  if [ $i -eq 18 ]; then continue; fi
  # if [ $i -eq 21 ]; then continue; fi
  # if [ $i -eq 27 ]; then continue; fi

  nodeid=`expr 2000 + $i`
  nickname=veasiroc$i
  dataport=`expr 9100 + $i` 
  sitcp_ip=192.168.11.$i
  module_num=$i

  if [ $i -ge 96 -a $i -le 102 ]; then
    $bin_dir/frontend.sh \
	$nickname \
	$nodeid \
	$dataport \
	$sitcp_ip \
	$module_num \
	$reg_dir \
	$adc_off \
	$tdc \
	>/dev/null 2>/dev/null &
  else
    $bin_dir/frontend.sh \
	$nickname \
	$nodeid \
	$dataport \
	$sitcp_ip \
	$module_num \
	$reg_dir \
	$adc \
	$tdc \
	>/dev/null 2>/dev/null &
  fi
      

done
