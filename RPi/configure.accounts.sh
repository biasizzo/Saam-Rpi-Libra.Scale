#!/bin/bash

echo "Enter username pi password:"
HASH_PI="$(openssl passwd -6)"
echo "HASH_PI=\"$HASH_PI\"" >> setup.cfg
echo "Enter username saam password:"
HASH_SAAM="$(openssl passwd -6)"
echo "HASH_SAAM=\"$HASH_SAAM\"" >> setup.cfg
KEYS=
# for file in ~/.ssh/id*.pub; do 
#   KEYS="${KEYS:+$KEYS\n}$(cat $file)";
# done
for file in ~/.ssh/id*.pub; do 
  KEYS="${KEYS}$(cat $file)\n";
done
while read key; do
  KEYS="${KEYS}${key}\n"
done < ~/.ssh/authorized_keys
echo "KEYS=\"$KEYS\"" >> setup.cfg
