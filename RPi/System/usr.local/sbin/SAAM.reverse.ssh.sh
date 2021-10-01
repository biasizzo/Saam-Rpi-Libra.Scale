#!/bin/bash

__second_instance="__second_instance_$$"
[[ -z ${!__second_instance} ]] && {
  declare -x "__second_instance_$$=true"
  exec -a SAAM.reverse.ssh.sh "/bin/bash" "$0" "$@"
}

SERIAL=`grep -i serial /proc/cpuinfo |cut -d ' ' -f 2`
LOCATION=`cat /boot/SAAM/location.id`
PORT=`echo "$LOCATION  $SERIAL" | ssh $SSH_TUNNEL_HOST './get_ssh_port.py'`

if [[ $PORT =~ ^[0-9]+$ ]]; then
   /usr/bin/autossh -M 0 -N -o ExitOnForwardFailure=yes \
                            -o ServerAliveInterval=60 \
                            -o ServerAliveCountMax=5 \
                            -R $PORT:localhost:22 $SSH_TUNNEL_USER@$SSH_TUNNEL_HOST
else
   logger -p user.warning -t ReverseSSH "Wrong reverse ssh port: $PORT"
   sleep 5
   exit -1
fi
