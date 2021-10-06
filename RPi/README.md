# SAAM Ambient sensor on Raspberry Pi

Developed within EU Horizon 2020 project SAAM.

## Install

  - Download and extract Raspberry OS image
    ```
    wget https://downloads.raspberrypi.org/raspios_lite_armhf/images/raspios_lite_armhf-2021-05-28/2021-05-07-raspios-buster-armhf-lite.zip
    unzip 2021-05-07-raspios-buster-armhf-lite.zip
    ```
  
  - Customize settings:
      - edit setup.env (set ssh tunnel host/user, update host/user, ...)
	  - run ./configure.accounts.sh:
	      - set RPi account passwords
		  - set ssh keys from local account
	```
	./configure.accounts.sh
	```

  - Modify Raspberry OS image
    ```
    dev=$(sudo losetup -Pf --show 2021-05-07-raspios-buster-armhf-lite.img)
    sudo mount ${dev}p1 /mnt
    sudo touch /mnt/ssh
    sudo umount /mnt
    sudo mount ${dev}p2 /mnt
    sudo sed -i-$(date -I).initial \
             -e "$ {x;s:^.*$:/root/setup.sh \> /root/setup.log 2\>\&1:;p;g}" \
             /mnt/etc/rc.local
    sudo rsync -a --chown=root:root setup.{env,sh} /mnt/root
    ssh-keygen -f ./id_ed25519 -t ed25519 -N ''
	sudo rsync -a --chown=root:root id_ed25519* /mnt/root
	rm id_ed25519
    sudo umount /mnt
    sudo losetup -d ${dev}
    ```

  - Write Raspberry OS image to the SD card\
    Check if the used raw device (/dev/sde, /dev/mmcblk0) really
    coresponds to the SD card device.
    ```
    sudo dd bs=4M if=2021-05-07-raspios-buster-armhf-lite.img of=/dev/sde
	```

  -	Copy public ssh keys to ssh tunnel server and update server.\
    If directly login to servers is not enabled, copy the keys using
    administrative account.
	```
    source setup.env
    source <(head -12 setup.sh|egrep "_(HOST|USER)=")
    (echo command="./get_ssh_port.py" '; cat id_ed25519.pub | \
      ssh "$SSH_TUNNEL_USER@$SSH_TUNNEL_HOST" "cat >> .ssh/authorized_keys"
    cat id_ed25519.pub | \
      ssh "$UPDATE_USER@$UPDATE_HOST" "cat >> .ssh/authorized_keys"
    ```
  
  - Insert SD card to Raspberry PI with MatrixIO Creator shield and boot\
    Final installation and configuration of Raspberry OS and applications are performed during first boot.\
	This step may take very long time (an hour or more) and at the end Rasperry PI reboots.

  - After second boot the applications have to be (manually) configured:
     - MQTT certificates, if used, must be copied to RPi.
     - MQTT authentication (username, password, certificates) must be configured in application configuration file:
	   transmit python dictionary in the ~saam/SAAM/sensor/settings.py 
		 - if MQTT broker uses TLS the 'SSL' key in the transmit dictionary must be defined
	 - Restart user services
	   ```
	   for srv in ~/.config/systemd/user/*service; do
         systemctl --user status $(basename $srv)
	   done
       ```
