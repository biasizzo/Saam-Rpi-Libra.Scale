# SAAM Ambient sensor on Raspberry Pi

Developed within EU Horizon 2020 project SAAM.

## Install

  - Download and extract Raspberry OS image
    ```
    wget https://downloads.raspberrypi.org/raspios_lite_armhf/images/raspios_lite_armhf-2021-05-28/2021-05-07-raspios-buster-armhf-lite.zip
    unzip 2021-05-07-raspios-buster-armhf-lite.zip
    ```
  
  - Modify and write Raspberry OS image to the SD card
    ```
    dev=$(sudo losetup -Pf --show 2021-05-07-raspios-buster-armhf-lite.img)
    sudo mount ${dev}p1 /mnt
    sudo touch /mnt/ssh
    sudo umount /mnt
    # Modify settings for SSH tunnel and update information in setup.cfg
    # Optionally set account (pi, saam) passwords and transfer ssh keys of the local account
    ./configure.accounts.sh
    sudo mount ${dev}p2 /mnt
    sudo sed -i-$(date -I).initial \
             -e "$ {x;s:^.*$:/root/setup.sh \> /root/setup.log 2\>\&1:;p;g}" \
             /mnt/etc/rc.local
    sudo rsync -a --chown=root:root setup.{cfg,sh} /mnt/root
    sudo rsync -a --chown=root:root opensmile-2.3.0.tar.gz /mnt/root
    sudo umount /mnt
    sudo losetup -d ${dev}
    sudo dd bs=4M if=2021-05-07-raspios-buster-armhf-lite.img of=/dev/sde
    ```
  
  - Insert SD card to Raspberry PI with MatrixIO Creator shield and boot
    On boot the system and applicationss are configured.

