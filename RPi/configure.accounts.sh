#!/bin/bash

for USER in pi saam; do
  for n in 1 2 3; do
    echo "Enter username ${USER} password:"
    HASH="$(openssl passwd -6)"
    [ $? -eq 0 ] || continue
    if [ "${HASH}" != "<NULL>" ]; then
      VAR="HASH_${USER^^}"
      sed -i -e "/^${VAR}=/{h;s|=.*$|='${HASH}'|}" \
             -e "$ {x;/^${VAR}=/{x;q};x;p;s|^.*$|${VAR}='${HASH}'|}" setup.env
    fi
    break
  done
  echo
done

KEYS=
eval $(grep "^KEYS=" setup.env)
for file in ~/.ssh/id*.pub; do 
  KEYS="${KEYS}$(cat $file)\n";
done
while read key; do
  KEYS="${KEYS}${key}\n"
done < ~/.ssh/authorized_keys
sed -ie "/^KEYS=/d" setup.env
echo "KEYS=\"$KEYS\"" >> setup.env
