#!/bin/bash

source /root/setup.env

SSH_TUNNEL_HOST=${SSH_TUNNEL_HOST:-ssh.tunnel.host}
SSH_TUNNEL_USER=${SSH_TUNNEL_USER:-ruser}
LOCAL_USER=${LOCAL_USER:-luser}
UPDATE_HOST=${UPDATE_HOST:-$SSH_TUNNEL_HOST}
UPDATE_USER=${UPDATE_USER:-$SSH_TUNNEL_USER}
HASH_PI=${HASH_PI:-'$6$4K6PWXyLA/O2yDms$fAk0QPh5r2m1Tkw.w06htSks0..sHW7zfbjuW5S/FqPmTmBpnUPwaTuqm2Uz.46Np3poAsjIsXdfeevz12OMu1'}
HASH_SAAM=${HASH_SAAM:-'$6$eV44s5I2Eh5.ieIY$W40Z5yVrOyvSV2jpAh.Vx/lnluRAJuye.vUe6sUggbNT1IuUdR0bjYu67d0wAB4HMsgaDmNKTAOpmO6By.qvn1'}

rsync -a /etc/rc.local "/root/rc.local-$(date).firstboot"
sed -i -e "/\/root\/setup\.sh/d" /etc/rc.local

DEB="$(sed -e "s/\..*$//" -e "/^[7-9]/s/$/.0/" /etc/debian_version)"

groupadd -g 1001 saam
useradd -c "SAAM user,,Application," -s /bin/bash \
        -g saam -G adm,sudo,audio,input,i2c,gpio,users \
        -p "$HASH_SAAM" -m -u 1001 saam
usermod -p "$HASH_PI" pi

mkdir -p ~pi/.ssh
printf "$KEYS" > ~pi/.ssh/authorized_keys
chmod 700 ~pi/.ssh
chown -R pi:pi ~pi/.ssh
rsync -a --chown=saam:saam ~pi/.ssh ~saam

orig="$(date -I).orig"
rsync -av /etc/bash.bashrc /etc/bash.bashrc-$orig
cat << EOF >> /etc/bash.bashrc

alias ls='ls --color=auto'
alias ll='ls -l'
alias la='ls -la'
EOF

LOC="Europe/Ljubljana"
echo "$LOC" > /etc/timezone
rm /etc/localtime
ln -s /usr/share/zoneinfo/${LOC} /etc/localtime
sed -i-$orig -e '/sl_SI\|bg_BG\|en_GB\|en_US\|de_AT/{/UTF/s/^[# ]*//}' /etc/locale.gen
locale-gen
sed -i-$orig -e "/^[# ]*NTP=/{s/^\([^#]\)/#\1/;h;s/^[# ]*//;s/=.*$/=ntp1.arnes.si/;H;x}" \
    /etc/systemd/timesyncd.conf
timedatectl set-timezone ${LOC}
timedatectl set-ntp false
TIME="$(wget -qSO- --max-redirect=0 google.com 2>&1 | grep Date: | cut -d' ' -f5-9)"
date -s "$TIME"
timedatectl set-ntp true

rsync -a /etc/fstab /etc/fstab-$orig
blkid|grep PARTUUID | \
   sed -e "s/^.*\(LABEL=\)\"/\1/" -e "s/\" .*\(PARTUUID=\)\"/ \1/" -e "s/\" *.*$//"  |\
   while read lbl uuid; do
      sed -i -e "s/$uuid */$lbl\\t/" /etc/fstab
   done

LOCATION_ID="/boot/SAAM/location.id"
mkdir -p /boot/SAAM
echo "JSI-E7" > $LOCATION_ID
cat << EOF > /usr/local/sbin/SAAM.local.sh
#!/bin/bash

LOCATION_ID="$LOCATION_ID"

if [ -f "\$LOCATION_ID" ]; then
   HOSTNAME=\`head -1 "\$LOCATION_ID"\`
   if [ -n "\$HOSTNAME" ]; then
      HOSTNAME="\${HOSTNAME}-amb"
      hostnamectl set-hostname "\$HOSTNAME"
      sed -i -e "/127\.0\.1\.1/s/\\([[:space:]]*\\)[[:alpha:]].*\$/\\1\$HOSTNAME/" /etc/hosts
   fi
fi
EOF

chmod 755 /usr/local/sbin/SAAM.local.sh
sed -i-$orig -e "\$i /usr/local/sbin/SAAM.local.sh" /etc/rc.local
. /usr/local/sbin/SAAM.local.sh

rsync -a /etc/wpa_supplicant/wpa_supplicant.conf \
         /etc/wpa_supplicant/wpa_supplicant.conf-$orig
echo "sensorlab" |wpa_passphrase "SAAM-AP" |\
    grep -v "#psk" >> /etc/wpa_supplicant/wpa_supplicant.conf
raspi-config nonint do_wifi_country SI

# MATRIX IO
wget https://apt.matrix.one/doc/apt-key.gpg -O- | sudo apt-key add -
echo "deb https://apt.matrix.one/raspbian $(lsb_release -sc) main" | \
    tee /etc/apt/sources.list.d/matrixlabs.list
# ZEROMQ
suse="https://download.opensuse.org/repositories"
zeromq="${suse}/network:/messaging:/zeromq:/release-stable/Debian_$DEB"
wget "$zeromq/Release.key" -O- | apt-key add -
echo "deb $zeromq/ ./" | tee /etc/apt/sources.list.d/zeromq.list

apt-get update
apt-get -y upgrade
apt-get -y install mc autossh matrixio-malos matrixio-kernel-modules \
        gfortran autoconf automake libtool portaudio19-dev \
        build-essential python-dev python3-pip avahi-utils \
        libatlas-base-dev espeak-ng libsndfile1 pulseaudio \
        symlinks libglib2.0-dev libusb-1.0-0-dev watchdog git cmake

pip3 install -q appdirs backports-abc certifi matrix_io-proto packaging \
                protobuf pyparsing pyzmq singledispatch six tornado \
                zerorpc paho-mqtt pyalsaaudio pandas scipy auditok \
                pynormalize scikit-learn==0.19.2 python_speech_features \
                watchdog PyAudio setproctitle liac_arff bluepy \
                func_timeout apscheduler persist-queue psutil

##################################################
###  MatrixIO shutdown workaround
##################################################
cwd=$(pwd)
cd /usr/share/matrixlabs/matrixio-devices
sed -e "/^flash bank/,/^#source/d" \
    -e "/^halt/,/^#reset/d" \
    -e "/^reset/s/run/halt/" \
    cfg/sam3s_rpi_sysfs.cfg > cfg/sam3s_halt.cfg
cd $cwd
cat << EOF > /lib/systemd/system-shutdown/matrix.creator.shutdown.sh
#!/bin/bash

[ "\$1" != "poweroff" ] && exit
/usr/bin/openocd -f /usr/share/matrixlabs/matrixio-devices/cfg/sam3s_halt.cfg
EOF
chmod a+x /lib/systemd/system-shutdown/matrix.creator.shutdown.sh

if [ -f "/etc/alsa/conf.d/20-bluealsa.conf" ]; then
  mv /etc/alsa/conf.d/20-bluealsa.conf \
     /etc/alsa/conf.d/._cfg00_20-bluealsa.conf-$orig
fi

groupadd -g 3001 ${LOCAL_USER}
useradd -c "Reverse SSH User,<email>,Local PI user for reverse ssh and update," \
        -g ${LOCAL_USER} -m -s /bin/false -u 3001 ${LOCAL_USER}
sudo -u ${LOCAL_USER} \
     ssh -o StrictHostKeyChecking=no -o BatchMode=yes \
         $SSH_TUNNEL_USER@$SSH_TUNNEL_HOST pwd >/dev/null 2>&1
sudo -u ${LOCAL_USER} \
     ssh -o StrictHostKeyChecking=no -o BatchMode=yes \
         $UPDATE_USER@$UPDATE_HOST pwd >/dev/null 2>&1
LOCAL_USER_SSH=$(eval echo "~$LOCAL_USER/.ssh")
echo "LOCAL_USER_SSH=$LOCAL_USER_SSH"
# sudo -u ${LOCAL_USER} ssh-keygen -q -t ed25519 -f ${LOCAL_USER_SSH}/id_ed25519 -N ''  # no passphrase !!!
rsync -a --chown=${LOCAL_USER}:${LOCAL_USER} /root/id_ed25519* ${LOCAL_USER_SSH}

#
# Configure autossh
#

cat << EOF > /etc/default/autossh
# Default settings for autossh tunnel

# Options to pass to autossh and/or ssh for tunneling
SSH_TUNNEL_USER=$SSH_TUNNEL_USER
SSH_TUNNEL_HOST=$SSH_TUNNEL_HOST
RPi_user=$LOCAL_USER
EOF

cat << EOF > /usr/local/sbin/SAAM.reverse.ssh.sh
#!/bin/bash

PORT=\`cat /boot/SAAM/location.id | ssh \$SSH_TUNNEL_HOST './get_ssh_port.py'\`

if [[ \$PORT =~ ^[0-9]+\$ ]]; then
   /usr/bin/autossh -M 0 -N -o ExitOnForwardFailure=yes \\
                            -o ServerAliveInterval=60 \\
                            -o ServerAliveCountMax=5 \\
                            -o PasswordAuthentication=no \\
                            -R \$PORT:localhost:22 \$SSH_TUNNEL_USER@\$SSH_TUNNEL_HOST
else
   sleep 5
   exit -1
fi
EOF
chmod 755 /usr/local/sbin/SAAM.reverse.ssh.sh

cat << EOF > /lib/systemd/system/autossh.service
[Unit]
Description=AutoSSH tunnel
Wants=network-online.target
After=network-online.target auditd.service
ConditionPathExists=!/etc/ssh/sshd_not_to_be_run

[Service]
EnvironmentFile=-/etc/default/autossh
ExecStart=/usr/bin/sudo -E -u ${LOCAL_USER} /usr/local/sbin/SAAM.reverse.ssh.sh
KillMode=mixed
Restart=always
RestartSec=60
Type=simple

[Install]
WantedBy=multi-user.target
EOF

systemctl enable autossh

##########################################################
### From updates !!!
##########################################################
GROUP_FOR_USERS="pulse-access:root,pi,saam; video:pi,saam"
# add root, pi, and saam to pulse-access group
usermod -a -G pulse-access root
usermod -a -G pulse-access pi
usermod -a -G pulse-access saam
# add pi, and saam to video group for omxplayer (accelerated)
usermod -a -G video pi
usermod -a -G video saam

loginctl enable-linger saam
setcap 'cap_net_raw,cap_net_admin+eip' /usr/local/lib/python3.*/dist-packages/bluepy/bluepy-helper
mkdir -p /var/log/SAAM
chown -R saam:saam /var/log/SAAM
ln -sf /home/saam/SAAM/Certificates/Production /opt/cert

/usr/local/sbin/change.timezone
SUDOERS_FILE="/etc/sudoers.d/010_saam-nopasswd"
systemctl daemon-reload
[ -f $SUDOERS_FILE ] || touch $SUDOERS_FILE
cat > $SUDOERS_FILE << EOF
saam ALL=(ALL) NOPASSWD: /usr/local/sbin/set.location
saam ALL=(ALL) NOPASSWD: /bin/systemctl start autossh.service
saam ALL=(ALL) NOPASSWD: /bin/systemctl stop autossh.service
saam ALL=(ALL) NOPASSWD: /bin/systemctl restart autossh.service
### USB power cycle
saam ALL=(ALL) NOPASSWD: /usr/local/sbin/uhubctl
EOF

###########################################
###  Install OpenSMILE 2.3.0
###########################################

mkdir -p ~/software
cd ~/software
git clone https://github.com/audeering/opensmile
cd opensmile
./build.sh
cmake --install build
mkdir -p /usr/local/share/opensmile
rsync -a --chown=root:root config /usr/local/share/opensmile
find /usr/local/share/opensmile/ -type d -exec chmod 755 {} \;
find /usr/local/share/opensmile/ -type f -exec chmod a+r {} \;
cd ~
rm -rf ~/software

#############################################
###  Install SAAM software
#############################################
GIT_ROOT="https://repo.ijs.si/toni" ### CHANGE !!!!
GIT_PROJECT="saam-rpi_libra.scale"
cd ~saam
sudo -u saam git clone "$GIT_ROOT/$GIT_PROJECT"
SYSTEM="${GIT_PROJECT}/RPi/System"
APPS="${GIT_PROJECT}/RPi/Applications"
rsync -a --chown=root:root "${SYSTEM}/usr.local/." /usr/local
rsync -a --chown=root:root "${SYSTEM}/etc.avahi.services/." /etc/avahi/services
rsync -a --chown=root:root "${SYSTEM}/services/system/." /lib/systemd/system
mkdir -p ~saam/.config/systemd/user
chown -R saam:saam ~saam/.config
rsync -a "${SYSTEM}/services/user/." ~saam/.config/systemd/user
mv ${APPS} ~saam/SAAM

cwd=$(pwd)
for mfile in ~saam/SAAM/*/Makefile; do
   cd $(dirname $mfile)
   sudo -u saam make
   make install
done
cd $cwd

sudo -u saam git clone https://github.com/mvp/uhubctl
cd uhubctl
sudo -u saam make 
prefix="/usr/local" make install
cd $cwd

cat << EOF >> /usr/local/etc/SAAM.update.conf

UPD_HOST="$UPDATE_HOST"
UPD_USER="$UPDATE_USER"
RPI_USER="$LOCAL_USER"
EOF

for srvc in ${SYSTEM}/services/system/*.service; do
   systemctl enable $(basename $srvc)
done

USER="saam"
RT_DIR="/run/user/$(id -u $USER)"
for srvc in ${SYSTEM}/services/user/*.service; do
   sudo -u $USER XDG_RUNTIME_DIR=$RT_DIR systemctl --user enable $(basename $srvc)
done

touch "/root/reboot-$(date)"
reboot
