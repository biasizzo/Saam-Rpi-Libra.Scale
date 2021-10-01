#!/bin/bash

#
# Change working directory to the place of script
#

SERVICE="SAAM.sensor"
TRASHOLD=50
# TRASHOLD=5
cd `dirname $0`
MONITOR=`basename $0 .sh`

while [ -f $MONITOR ]; do
  # USAGE=`ps -o %mem,command ax |grep python|grep matrix|sort -b -n -k1|tail -1|sed -e "s/^ *//"|cut -d " " -f 1|sed -e "s/\..*$//"`
  USAGE=`ps -o %mem,command ax |grep Ambient|grep -v grep|sort -b -n -k1|tail -1|sed -e "s/^ *//"|cut -d " " -f 1|sed -e "s/\..*$//"`
  if [ -n "$USAGE" ]; then
    # echo "`date`:  Memory usage: $USAGE%"
    if [ $USAGE -gt $TRASHOLD ]; then
      # echo "Do restart"
      systemctl restart $SERVICE
    fi
  fi
  sleep 300
done
