#!/bin/bash

ACQUISITION="./matrix_creator.py"
SAAMWAIT="/usr/local/etc/SAAM.wait"
SAAMDELAY=10

RETRY=5

#
# SAAM Watchdog - if SAAM app craches the system
#
while [ -f "${SAAMWAIT}" ]; do
  [ ${RETRY} -lt 1 ] &&  exit 0
  RETRY=$((RETRY-1))
  sleep ${SAAMDELAY}
done

#
# SAAM sensor conditioning
#
cd `dirname $0`

[ -f do_not_acquire ] && sleep ${SAAMDELAY} && exit 0

#
# Starting application
#

${ACQUISITION}
