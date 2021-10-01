# SAAM project PC BLE interface

This is a placeholder for full README when bugs are ironed out.

## **LICENSE TO BE DECIDED**

This code is NOT to be used outside SAAM project and is without any warranty, until this file changes

## Dependencies
 - python 3.4 (or higher)
 
 - bluepy - https://github.com/IanHarvey/bluepy

 - paho-mqtt - https://pypi.org/project/paho-mqtt/ 

 - func-timeout - https://pypi.org/project/func-timeout/ - this module seems to work better than other in combination with multiprocessing & ble peripheral objects

Using pip3 install all necessary dependecies with: 

`pip3 install bluepy paho-mqtt func-timeout --user`


## Usage
Run main entry with path to config file (see below how to set up config file)
~~~
./runSam.py _path_to_config_.json
~~~
(or prepend with sudo/python3, depending on your configuration)

#### Wait, this runs indefinitely?
Yes, in runSaam.py there is a `while true` loop that waits indefinitely for messages. Substitute this with timeout if you so desire.  

For deployments we recommend running it as a service (systemd, wrapper script..), that restarts runSaam script whenever it stops. 

### More detailed device state
If you wish to see more data and real time state of the device, check log files under /var/log/SAAM/**


## Config file

Example of config file below:
~~~

{
  "location": "/etc/lgtc/loc-id",
  "topic": "saam/data",
  "mqtt_on": true,
  "mqtt_timestep":10,
  "mqtt_ip": "ip",
  "mqtt_port": port,
  "mqtt_user": "username",
  "mqtt_pwd": "password",
  "devices": [
    {
      "type": "microhub",
      "hubType":"BED",
      "mac": "e0:7d:ea:ef:12:34",
      "location": "sens_bed",
      "comment": "BED sensor"
    },
    {
      "type": "microhub",
      "hubType":"CLIP",
      "mac": "e0:7d:ea:ef:12:34",
      "location": "sens_belt",
      "comment": "BELT sensor"
    },
    {
      "type": "microhub",
      "hubType":"WRIST",
      "mac": "9c:a5:25:12:b7:0d",
      "location": "sens_bracelet_left",
      "comment": "WRIST sensor"
    }
  ]
}

~~~

# BLE on linux & Bluepy instructions

### Optional
Check BLE support of your device issuing `hciconfig -a`, and check that line "*HCI Version:*" reports "4.0" or higher


## Installation
 - Follow instructions on https://github.com/IanHarvey/bluepy
   - we suggest installing bluepy using `pip install bluepy --user` command, to avoid any additional "sudo" configuration details

### [Optional, but recommended] Using BluePy as (non-sudo) linux user
Locate *bluepy-helper* file (if bluepy is installed using pip --user command, it is usually installed under ~/.local/lib/python3.*/site-packages/bluepy/ )

Go to the the directory and issue command:

`
sudo setcap 'cap_net_raw,cap_net_admin+eip' bluepy-helper
`

This will enable running BluePy BLE scan functionality as a normal linux user.

