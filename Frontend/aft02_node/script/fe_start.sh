#!/bin/sh

bin_dir=$(dirname `readlink -f $0`)

cd $bin_dir

$bin_dir/message.sh > /dev/null 2> /dev/null &

sleep 1

reg_dir=${HOME}/vme-easiroc-registers
#reg_dir=default
adc=on
tdc=on

#for i in $(seq 69 69)
#for i in $(seq 27 30) $(seq 44 48) $(seq 64 68) $(seq 86 89)
#for i in $(seq 27 30) $(seq 44 48) $(seq 64 68) $(seq 86 86)
#for i in $(seq 27 30) $(seq 44 48) $(seq 64 68) 
# for i in $(seq 27 30) $(seq 44 48) $(seq 64 68) $(seq 86 86)
for i in $(seq 52 53) $(seq 69 69) $(seq 27 30) $(seq 44 45) #2024.03.22
#for i in $(seq 52 53) $(seq 69 69) $(seq 27 29) $(seq 44 45) #2024.04.21
# for i in $(seq 27 30) $(seq 44 48) $(seq 64 68)
# for i in $(seq 86 86)

#for i in $(seq 16 17) $(seq 31 34) $(seq 49 53) $(seq 69 69) $(seq 27 30) $(seq 44 48) $(seq 64 68) $(seq 86 86)

do
  if [ $i -eq 18 ]; then continue; fi

    nodeid=`expr 2200 + $i`
    nickname=aft02-$i
    dataport=`expr 9100 + $i`
    sitcp_ip=192.168.11.$i
    module_num=$i

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
done
