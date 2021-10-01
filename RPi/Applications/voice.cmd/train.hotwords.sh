#!/bin/bash

cd `dirname $0`
[ -f do_training ] || exit 0
VLANG=`hostname|tr 'A-Z' 'a-z'`
VLANG=${VLANG:0:2}
echo "$VLANG"|grep -q "si\|bg\|en\|de" || VLANG="en"
# echo "$VLANG"|grep -q "si\|en" || VLANG="en"

./voice.train.py -l "$VLANG" -O
