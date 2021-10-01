#!/usr/bin/python3

import sys, os
import time as sys_time
from os.path import isfile as os_is_file
from datetime import datetime, timezone

from math import isnan
import json
from settings import run, transmit, report

def mqtt_init(configuration):
    mqtt = dict()
    get_list = lambda x: x if isinstance(x, list) else [x]
    channels = {chn for dev in configuration['devices'] for chn in get_list(dev['channels'])}
    for name, cfg in transmit.items():
        if name not in channels:
            continue
        try:
            if cfg and 'TYPE' in cfg and cfg['TYPE'] == 'MQTT': # Valid configuration
                mqtt['topic']     = cfg['TOPIC']
                mqtt['mqtt_on']   = True
                mqtt['mqtt_ip']   = cfg['HOST']
                mqtt['mqtt_port'] = cfg['PORT']
                mqtt['mqtt_keepalive'] = cfg['KEEPALIVE']
                mqtt['mqtt_user'] = cfg['USER']
                mqtt['mqtt_pwd']  = cfg['PASS']
                mqtt['mqtt_ca']   = cfg['SSL']['CA']
                mqtt['mqtt_cert'] = cfg['SSL']['CERT']
                mqtt['mqtt_key']  = cfg['SSL']['KEY']
                break;
        except:
            pass
    return mqtt


def read_config_files(files):
    config = dict()
    for cfg_file in files:
        with open(cfg_file) as json_file:
            config.update(json.load(json_file))
    devices = []
    if 'devices' in config and type(config['devices']) == dict:
        if run['LOCATION_ID'] in config['devices']:
            devices = config['devices'][run['LOCATION_ID']]
     
    if len(devices) < 1:
        sys.exit(0)
    for dev in devices:
        dev["mac"] = dev["mac"].lower()
        upd = dict(precision=3, channels=[], period=10)
        try:
            desc = report[dev['location']]
            upd['precision'] = desc[2]
            upd['channels']  = desc[3]
            upd['period']    = desc[1]
            if not isinstance(upd[channel], list):
                upd['channels'] = [upd['channels']]
        except:
            pass
        dev.update(upd)
        
    config['devices'] = devices
    return config



if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage\n  ble_config.py out_file in_file [in_files]")
    os.chdir(os.path.dirname(sys.argv[0]))
    try:
        os.remove(sys.argv[1])
    except:
        pass
    ################## DEBUG ####################
    # run['LOCATION_ID'] = "SI03"
    #############################################
    config = {'location': '/boot/SAAM/location.id'}
    configuration = read_config_files(sys.argv[2:])
    mqtt_channels = mqtt_init(configuration)
    if not mqtt_channels:
        sys.exit(0)
    for dev in configuration['devices']:
        config['mqtt_timestep'] = dev['period']
        break
    config.update(mqtt_channels)
    config['devices'] = configuration['devices']
    with open(sys.argv[1], "w") as json_file:
        json.dump(config, json_file, indent=4)
    # print(json.dumps(config, indent=4))
    

