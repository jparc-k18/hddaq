#!/bin/sh

bin_dir=$(cd $(dirname $0); pwd -P)/../../../Message/bin
msgd=$bin_dir/msgd

if [ $$ -ne $(pgrep -fo $0) ]; then
    echo "$0 is already running."
    exit
fi

while true
do
    $msgd
    sleep 1
done
