#!/bin/sh

script_dir=$(dirname `readlink -f $0`)
bin_dir=$script_dir/../bin
frontend=aft03_frontend

if [ $# != 8 ]; then
    echo "Usage : $(basename $0) --nickname=NICK_NAME --nodeid=NODE_ID --data-port=DATA_PORT --sctcp-ip=SiTCP_IP --module-num=Number --reg-dir=dir_path --adc=on/off --tdc=on/off"
    exit
fi

nickname=$1
nodeid=$2
dataport=$3
sitcp_ip=$4
module_num=$5
reg_dir=$6
adc=$7
tdc=$8

while true
do
$bin_dir/$frontend \
    --nickname=$nickname  \
    --nodeid=$nodeid      \
    --data-port=$dataport \
    --sitcp-ip=$sitcp_ip \
    --module-num=$module_num \
    --reg-dir=$reg_dir \
    --adc=$adc \
    --tdc=$tdc
    # --run-number-nolock

    sleep 1
done
