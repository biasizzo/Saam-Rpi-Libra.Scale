#######################################################################
#  BASH functions to aid update script                                #
#######################################################################

# users=`cut -d ":" -f 1 /etc/passwd`
# groups=`cut -d ":" -f 1 /etc/group`

user="saam"
uid=$(id -u $user)
xdg="/run/user/$uid"
user_cmd="sudo -u $user XDG_RUNTIME_DIR=$xdg systemctl --user"

function get_uninstalled_pkgs() {
  pkgs=""
  while [ -n "$1" ]; do
    installed=`dpkg -l|grep "^ii"|sed -e "s/  */ /g"|cut -d " " -f 2|grep "^$1$"`
    if [ "$1" != "$installed" ]; then
      [ -z "$pkgs" ] && pkgs="$1" || pkgs="$pkgs $1"
    fi
    shift
  done
  echo "$pkgs"
}

function install_pkgs() {
  LOG=$1
  shift
  [ -z "$1" ] && return 0 # Empty package list - shoold not happen
  # Prepare for update
  apt-get update -qq 2>> $LOG 
  # apt-get upgrade -qq 2>> $LOG ### Is it necessarry?
  apt-get install -qq $@ >> $LOG 2>&1
  return $?
}

function get_uninstalled_pip_pkgs() {
  inst_pkgs=`pip3 list --format=freeze|sed -e "s/=.*$//"`
  pkgs=""
  while [ -n "$1" ]; do
    echo $inst_pkg|tr " " "\n"|grep -q "^$1$" || pkgs="$pkgs $1"
    shift
  done
  echo "$pkgs"
}

function install_pip_pkgs() {
  LOG=$1
  shift
  [ -z "$1" ] && return 0 # Empty package list - shoold not happen
  yes | pip3 install --progress-bar off $@ >> $LOG 2>&1
  return $?
}

# Attach group to users defined in input variable ($1):
#   - group1:user1,user2,..userN; group2:userA,userB,...
function attach_groups_to_user() {
  if [ -n "$1" ]; then
    echo "Original cfg string: $1"
    for grp_desc in `echo "$1"|sed -e "s/; */ /"`; do
      group="${grp_desc/%:*/}"
      echo "$group <> ${grp_desc/#*:/}"
      cut -d ":" -f 1 /etc/group | grep -q "^$group$" || continue
      for user in `echo "${grp_desc/#*:/}"|tr ',' ' '`; do
        cut -d ":" -f 1 /etc/passwd | grep -q "^$user$" || continue
        echo usermod -a -G $group $user
        usermod -a -G $group $user
      done
    done
  fi
}

# Add groups to a user defined by input variable ($1):
#   - user1:group1,group2,...groupM; user2:groupA,groupB,...
function add_groups_to_user() {
  if [ -n "$1" ]; then
    for user_desc in `echo "$1"|sed -e "s/; */ /"`; do
      user="${user_desc/%:*/}"
      echo "$user >< ${user_desc/#*:/}"
      cut -d ":" -f 1 /etc/passwd | grep -q "^$user$" || continue
      addgroup=""
      for group in `echo "${user_desc/#*:/}"|tr ',' ' '`; do
        cut -d ":" -f 1 /etc/group | grep -q "^$group$" && addgroup="$addgroup$group,"
      done
      echo usermod -a -G ${addgroup/%,/} $user
      usermod -a -G ${addgroup/%,/} $user
    done
  fi
}

function enable_services() {
  for srv in $1; do
    systemctl enable $srv >/dev/null 2>&1
    systemctl start  $srv >/dev/null 2>&1
  done
}

function disable_services() {
  for srv in $1; do
    systemctl stop $srv >/dev/null 2>&1
    systemctl disable  $srv >/dev/null 2>&1
  done
}

function start_services() {
  for srv in $1; do
    enabled=`systemctl status $srv 2>/dev/null|grep "; *enabled *"`
    [ -n "$enabled" ] && systemctl start  $srv >/dev/null 2>&1
  done
}

function restart_services() {
  for srv in $1; do
    enabled=`systemctl status $srv 2>/dev/null|grep "; *enabled *"`
    [ -n "$enabled" ] && systemctl restart  $srv >/dev/null 2>&1
  done
}

function stop_services() {
  for srv in $1; do
    systemctl stop  $srv >/dev/null 2>&1
  done
}

function running_services() {
  services=""
  for srv in $1; do
    enabled=`systemctl status $srv 2>/dev/null|grep Active|grep "(running)"`
    [ -n "$enabled" ] && services="$services $srv"
  done
  echo $services
}
