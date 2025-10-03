#!/bin/sh

bin_dir=$(dirname `readlink -f $0`)

cd $bin_dir

$bin_dir/message.sh > /dev/null 2> /dev/null &
#$bin_dir/message.sh &

sleep 1

reg_dir=${HOME}/vme-easiroc-registers
#reg_dir=default
adc=on
adc_off=off
tdc=on


#for i in $(seq 16 17) $(seq 31 34) $(seq 49 51) # 2024.03.22
for i in 65 66 67 68 86 # 2025.09.14

do
  # if [ $i -eq 18 ]; then continue; fi
  # if [ $i -eq 21 ]; then continue; fi
  # if [ $i -eq 27 ]; then continue; fi

#  echo "$i"
  nodeid=`expr 2300 + $i`
  nickname=rc01-$i
  dataport=`expr 9000 + $i`
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
