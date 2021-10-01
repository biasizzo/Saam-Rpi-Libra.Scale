#!/bin/bash

[ -f /usr/local/etc/SAAM.no.update ] && exit 0

UPD_BASE=SAAM
UPD_HOST="...update_host.fqdn..."    # Could be the same as SSH tunnel host
UPD_USER="...update_user..."         # Could be the same as SSH tunnel user
RPI_USER="...RPi_user..."            # Could be the same as local user for SSH tunnel
RUN=/run/SAAM.update.pid
LOG=/root/update/update.log
UPDATES=`dirname $LOG`/files
CRT=/usr/local/etc/SAAM.Ambient.Update.pem
DEPLOYED="/home/saam/SAAM"
RELOAD=0
SYS_DESC=~saam/SAAM/system.desc

TMPDIR="/tmp/SAAM.update.$$"

[ -f $RUN ] && exit 0

[ -f /usr/local/etc/SAAM.update.conf ] && . /usr/local/etc/SAAM.update.conf

UPD_DIR="${UPD_USER}@${UPD_HOST}:${UPD_BASE}"
SSH_ID="~${UPD_USER}/.ssh/id_ed25519"

function cleanup() {
  SINCE_BOOT=0
  [ -f $TMPDIR/reboot ] && SINCE_BOOT=$(sed -e "s/ .*$//" -e "s/\..*$//" /proc/uptime)
  rm -rf $RUN $TMPDIR /run/SAAM.update
  if [ $SINCE_BOOT -gt 86400 ]; then  # reboot if requested and not rebooted for one day
    sync
    shutdown -r now
  fi
}

function service_restart() {
  for srv in $1; do
    systemctl -q is-enabled && echo "Service $srv restarted" >> $LOG && 
      systemctl restart $srv || echo "Service $srv failed" >> $LOG
  done
}

function manage_services() {
  echo "In Manage service $*" >> $LOG
  sysctl="systemctl"
  suffix=
  if [ "$1" == "user" ]; then
    sysctl="sudo -u saam XDG_RUNTIME_DIR=/run/user/1001 systemctl --user"
    suffix="_USER"
  fi
  echo "systemctl command: $sysctl"
  declare -A sys_srv
  nid=1
  [ $RELOAD -gt 0 ] && $sysctl daemon-reload
  for action in ENABLE START RESTART STOP DISABLE; do
    action="${action}${suffix}"
    [ -n "${!action}" ] && echo "$action: ${!action}" >> $LOG
    for srv in ${!action}; do
      sys_srv[${srv}]=$((sys_srv[${srv}] | nid))
    done
    nid=$((nid<<1))
  done
  for srv in ${!sys_srv[@]}; do
    [ $((sys_srv[$srv] & 16)) -gt 0 ] && sys_srv[$srv]=$((sys_srv[$srv] | 8))  # STO
    flag=$(((sys_srv[$srv] >> 1) & 2))
    sys_srv[$srv]=$((sys_srv[$srv] | (flag & (sys_srv[$srv] << 1))))
    if $sysctl -q is-enabled $srv; then # enabled 
      sys_srv[$srv]=$((sys_srv[$srv] | flag))
    fi
    flag=$(((8 & sys_srv[$srv] ^ 8) >> 1))
    # if $sysctl -q is-active $srv; then  # running
    #   sys_srv[$srv]=$((sys_srv[$srv] & (flag | 25)))          # STA RES
    # else
    #   sys_srv[$srv]=$((sys_srv[$srv] & ((flag >> 1) | 25)))   # STA RES
    # fi
    case `$sysctl is-active $srv` in
      activ*|restart* )
        sys_srv[$srv]=$((sys_srv[$srv] & (flag | 25)))          # STA RES
        ;;
      * )
        sys_srv[$srv]=$((sys_srv[$srv] & ((flag >> 1) | 25)))   # STA RES
        ;;
    esac
    sys_srv[$srv]=$((sys_srv[$srv] & (31 ^ (sys_srv[$srv] >> 4))))  # EN
    ### SAAM.update should not be started, stopped, or restarted
    # [[ $srv == SAAM.update* ]] && sys_srv[$srv]=$((sys_srv[$srv] & 17))

    num=${sys_srv[$srv]} 
    for action in ENABLE START RESTART STOP DISABLE; do
      [ $num -eq 0 ] && break
      if [ $((num & 1)) -gt 0 ]; then
        echo "$sysctl ${action,,} $srv" >> $LOG
        $sysctl ${action,,} $srv 2>&1 >> $LOG
      fi
      num=$((num >> 1))
    done
  done
}


function do_rsync() {
  RLOG=$TMPDIR/rsync.log
  echo rsync "${1}" -v -e "ssh -i $SSH_ID" "${UPD_DIR}/$2" "$3" >> $LOG
  rsync ${1} -v --info=stats0,flist0 -e "ssh -i $SSH_ID" "${UPD_DIR}/$2" "$3" > $RLOG 2>&1
  cat $RLOG >> $LOG
  upd_srv=`grep "\.service$" $RLOG`
  echo "Updated services:  $upd_srv" >> $LOG
  [ -n "$upd_srv" ] && RELOAD=1

  CHECK_SRVC=0
  for base_dir in $DEPLOYED; do
    [[ $3 == ${base_dir}* ]] && CHECK_SRVC=1 && break
  done
  unset ref rev
  declare -A ref
  declare -A rev
  if [ $CHECK_SRVC -gt 0 ]; then
    while read dummy1 link dummy2 file rest; do
      file="`dirname $link`/$file"
      while [[ $file == */..* ]]; do
        file="${file//+([^\/])\/..\//}"
      done
      [[ $file == ${3}/* ]] || continue
      link="${link#${3}/}"
      file="${file#${3}/}"
      rev[$link]=$file
    done < <(symlinks -rv $3|grep "^relative") #####
   
    for link in ${!rev[@]}; do
      file=${rev[$link]}
      while [ -n "$file" ]; do
        ref[$file]="${ref[$file]:+${ref[$file]} }${link}"
        file=${rev[$file]}
      done
    done
  fi

  updated=`grep "\.py$" $RLOG | sed -e "s:^$3/::" -e "s/  *-> .*$//"|while read file; do echo $file; for link in ${ref[$file]}; do echo $link; done; done|sed -e "s:^$3/::" -e "s:/.*$::"|uniq`
  upd_srv="${upd_srv} `for d in $updated; do ls -1 $3/$d/*.service 2>/dev/null; done`"
  # srvc=`for s in $upd_srv; do echo "$s"; done|sed -e "s/^.*\///"|sort -u`
  # echo $upd_srv|tr " " "\n"|sed -e "s/^.*\///"|sort -u
  upd_srv=`echo $upd_srv|tr " " "\n"|sed -e "s/^.*\///"|sort -u|grep -v "^$"`
  RESTART="${RESTART:+${RESTART} }${upd_srv//+([[:space:]])/ }"
}

function do_update() {
  [ ! -f $CRT ] && echo "Update failed: no certificate" >> $LOG && return
  rsync -a $UPDATES/. $TMPDIR > /dev/null 2>&1
  [ $? -ne 0 ] && echo "Update failed: initial rsync failed" >> $LOG && return
  RLOG="$TMPDIR/rsync.log"
  SPLIT="$TMPDIR/split_"
  PATTERN="### Signature"
  # echo "update $1" >> $LOG
  rsync -auv --info=stats0,flist0 -e "ssh -i $SSH_ID" "${UPD_DIR}/$1" $TMPDIR > $RLOG 2>&1
  cat $RLOG >> $LOG
  UPD_FILES=`grep "\.sh$" $RLOG|sort -u`
  for upd in $UPD_FILES; do
    csplit -f $SPLIT $TMPDIR/$upd "/$PATTERN/"
    [ ! -f ${SPLIT}01 ] && echo "Update $upd failed: no signature" >> $LOG && continue
    openssl dgst -sha256 -verify $CRT -signature <(tail -n +2 ${SPLIT}01|sed -e "s/^# //"|base64 -d) ${SPLIT}00 > /dev/null
    [ $? -ne 0 ] && echo "Update $upd failed: wrong signature" >> $LOG && continue
    echo -e "\nUpdating $upd" >> $LOG
    # $UPDATES/$upd 2>&1 |tee $RLOG >> $LOG  ### Updated: 29 May 2020
    $TMPDIR/$upd > $RLOG 2>&1
    ERROR=$?
    cat $RLOG >> $LOG
    [ $ERROR -ne 0 ] && echo "Update $upd failed: script error" >> $LOG && continue
    rsync -au $TMPDIR/$upd $UPDATES > /dev/null 2>> $LOG
    echo -e "---------------------------------------------------\n" >> $LOG
    # Determine _SRVC variables in update them
    while read line; do
      # echo "$line" >> $LOG
      name=${line%_SRVC=*}
      line=${line#*=}
      printf -v $name "${!name:+${!name} }${line//+([[:space:]])/ }"
      echo "$name: ${!name}" >> $LOG
    done < <(grep "^[_A-Z]*_SRVC=" $RLOG)
  done
}

# Initial signature to detect script change
orig_cksum=$(cksum $0|cut -f 1 -d " ")

trap cleanup SIGHUP SIGINT SIGQUIT SIGABRT SIGTERM EXIT 

echo "$$" > $RUN
mkdir -p $UPDATES
mkdir -p $TMPDIR
echo "UPDATE @ `date` --- cksum:${orig_cksum}" >> $LOG
uptime >> $LOG
uptime=`cat /proc/uptime|sed -e "s/[\. ].*$//"`

n=600
while ! host $UPD_HOST >> $LOG 2>&1;  do
  [ $n -le 0 ] && break
  n=$((n-1))
  sleep 1
done


if [ $uptime -lt 100 ]; then
  echo "Update time from http source (google)" >> $LOG
  date -s "$(wget -qSO- --max-redirect=0 www.google.com 2>&1 >/dev/null| grep Date: | cut -d' ' -f5-9)"
  echo "CORRECTED UPDATE TIME: `date`" >> $LOG
fi

n=60
scp -i $SSH_ID ${UPD_DIR}/update.cmd $TMPDIR >> $LOG 2>&1
while [ $? -ne 0 ]; do
  [ $n -le 0 ] && break
  n=$((n-1))
  sleep 10
  scp -i $SSH_ID ${UPD_DIR}/update.cmd $TMPDIR >> $LOG 2>&1
done

if [ ! -f $TMPDIR/update.cmd ]; then
  cleanup
  exit -1
fi

services=""
shopt -s extglob
while read cmd arg1 arg2 arg3 arg4 arg5 arg6; do
  case $cmd in
    rsync)
      do_rsync "$arg3 $arg4 $arg5 $arg6" "$arg1" "$arg2"
      ;;
    update)
      do_update "$arg1"
      ;;
    *)
      echo "Unknown command $cmd" >> $LOG
      ;;
  esac
  new_cksum=$(cksum $0|cut -f 1 -d " ")
  if [ $orig_cksum  -ne $new_cksum ]; then
    rm -f $RUN 2>/dev/null
    echo "UPDATE script changed!!!" >> $LOG
    echo >> $LOG
    $0 $*
    echo "Newer update script done" >> $LOG
    echo "$$" > $RUN
    exit 0
  fi
done < <(grep -v "^ *#" $TMPDIR/update.cmd)

manage_services >> $LOG
RESTART_USER="${RESTART}"
manage_services user >> $LOG
shopt -u extglob
touch $SYS_DESC
if grep -q "^ *Update.Dir *:" $SYS_DESC; then
  sed -i "/^ *Update.Dir *:/s/: *.*$/: ${UPD_BASE}/" $SYS_DESC
else
  echo "Update.Dir: ${UPD_BASE}" >> $SYS_DESC
fi

for upd in $UPD_FILES; do
  ftime=`ls -g -o --time-style="+%F_%T_%Z" $UPDATES/$upd| \
         sed -e "s/  */ /g"|cut -d " " -f 4|tr "_" " "`
  if grep -q "^ *Update.File *: *$upd" $SYS_DESC; then
    sed -i "/^ *Update.File *: *$upd/s/: *.*$/: $upd [$UPD_BASE] @ $ftime/" $SYS_DESC
  else
    echo "Update.File: $upd [$UPD_BASE] @ $ftime" >> $SYS_DESC
  fi
done
  
chown saam:saam $SYS_DESC

echo "" >> $LOG

cleanup

