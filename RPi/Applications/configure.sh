#!/bin/bash

cd $(dirname $0)

#
# BLE configuration
#
saam-ble/ble_config.py config.json ble_devices.json

exit 0
