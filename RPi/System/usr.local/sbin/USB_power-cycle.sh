#!/bin/bash

read model < /proc/device-tree/model
model=`echo $model|sed -e "s/Raspberry Pi *//" -e "s/ *Model *//" -e "s/ *Plus/+/"`
rev=`echo $model|sed -e "s/^.*Rev *//"`

shopt -s extglob
case $model in
  3B?(+)" Rev 1."[3-9]*)
      uhubctl -l 1-1 -p 2 -a 2
      ;;
  "4B Rev 1."[1-9]*)
      uhubctl -l 1-1 -a 2
      ;;
esac
