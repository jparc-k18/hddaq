#!/bin/sh

bin_dir=$(dirname `readlink -f $0`)

${bin_dir}/message.sh >/dev/null 2>/dev/null &

sleep 1;

${bin_dir}/frontend.sh >/dev/null 2>/dev/null &
# ${bin_dir}/frontend.sh
