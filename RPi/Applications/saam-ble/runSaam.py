#!/usr/bin/python3

import sys
import time as sys_time
from os.path import isfile as os_is_file
from datetime import datetime, timezone

from math import isnan

from COMMON_abst.device import MsgType, Device, DeviceQueue
from COMMON_abst.logger import get_logger
from COMMON_abst.scan_thread import ScannerWorker
from COMMON_abst.config_parser import parse_saam_config_json
from IAW_microhub.microhub import Microhub, MicroHubType
from COMMON_blood_pressure_monitor.blood_pressure_monitor import BPMonitor

import paho.mqtt.client
from paho.mqtt.client import MQTT_ERR_SUCCESS

import json as pyjson


def h_time():
    return datetime.now(timezone.utc).strftime("%Y_%m_%d %T")


def mqtt_init(config_mqtt, log):
    # Create and handle mqtt client
    # TODO move this to seperate file / Thread?
    if config_mqtt.mqtt_on:
        _mqtt_client = paho.mqtt.client.Client()
        if log:
            log.debug("{}  MQTT Initialization started!".format(h_time()))
            _mqtt_client.enable_logger(log)

        _mqtt_client.username_pw_set(
            config_mqtt.mqtt_user, password=config_mqtt.mqtt_pwd
        )
        _mqtt_client.tls_set(
            ca_certs=config_mqtt.mqtt_ca,
            certfile=config_mqtt.mqtt_cert,
            keyfile=config_mqtt.mqtt_key,
        )
        _mqtt_client.tls_insecure_set(True)

        log.debug("{}  calling MQTT connect!".format(h_time()))
        # if no internet connection, this WILL throw, and everything will be fine, sampling of devices will NOT start
        _mqtt_client.connect(config_mqtt.mqtt_ip, port=config_mqtt.mqtt_port)
        _mqtt_client.loop_start()  # keep connection active

        if log:
            log.debug("{}  MQTT Initialization finished!".format(h_time()))
        print("MQTT conection is set!")
        return _mqtt_client
    if log:
        log.debug("{}  MQTT not initalizing!".format(h_time()))
    return None


def mqtt_disconect(client):
    if client:
        client.disconnect()


def mqtt_publish(client, topic, msg, log):
    if client:
        success = client.publish(str(topic), pyjson.dumps(msg))
        if success.rc != MQTT_ERR_SUCCESS:
            if log:
                log.debug(
                    " {} Mqtt message sending failed: {}".format(h_time(), str(success))
                )
        return success
    return -1


def get_mqtt_topic_from_config(config, device, data_id):
    device_name = ""
    for d in config.devices:
        if d["mac"] == device.deviceMac:
            device_name = d["location"]
            break
    return (
        config.topic
        + "/"
        + config.location
        + "/"
        + device_name
        + "_"
        + data_id
        + "_amb"
    )


def startDeviceThread(deviceInfo, queue: DeviceQueue, log=None):
    if deviceInfo["type"] == "microhub":
        mtype = None
        if deviceInfo["hubType"] == "BED":
            mtype = MicroHubType.BED
        if deviceInfo["hubType"] == "WRIST":
            mtype = MicroHubType.WRIST
        if deviceInfo["hubType"] == "CLIP":
            mtype = MicroHubType.CLIP

        dev = Microhub(mtype, deviceInfo["mac"], queue, name=deviceInfo["location"])
        dev.startDevice()
        if log:
            log.debug(
                "%s Starting uHub device %s %s",
                h_time(),
                deviceInfo["location"],
                deviceInfo["mac"],
            )
        return dev
    if deviceInfo["type"] == "bpm":
        dev = BPMonitor(deviceInfo["mac"], queue)
        dev.startDevice()
        if log:
            log.debug(
                "%s Starting uHub device %s %s",
                h_time(),
                deviceInfo["location"],
                deviceInfo["mac"],
            )
        return dev
    if deviceInfo["type"] == "savvy":
        # do nothing currently, savvy not deployed to SAAM sites
        pass


def handleDeviceSpecificData(config, device, data, mqtt_instance, log=None):
    if data and (device.deviceType == "bpm"):
        try:

            def _timestamp_from_datetime(d):
                from datetime import datetime, timedelta
                from pytz import timezone as ptz

                # get timezone in regards to location
                curr_timezone = ptz("UTC")  # default is UTC...
                if config.location.startswith("BG"):
                    curr_timezone = ptz("Europe/Sofia")
                if config.location.startswith("AT"):
                    curr_timezone = ptz("Europe/Vienna")
                if config.location.startswith("SI"):
                    curr_timezone = ptz("Europe/Ljubljana")
                dt = datetime(
                    d["date"][0],
                    d["date"][1],
                    d["date"][2],
                    d["time"][0],
                    d["time"][1],
                    d["time"][2],
                )
                aware_d = curr_timezone.localize(dt, is_dst=None)
                utc_d = aware_d.astimezone(ptz("UTC"))
                result = utc_d - datetime(1970, 1, 1, 0, 0, 0, tzinfo=timezone.utc)
                # print(
                #    d["date"],
                #    d["time"],
                #    "->",
                #    result,
                #    "==",
                #    result / timedelta(seconds=1),
                #    "\n",
                #    datetime.utcfromtimestamp(result / timedelta(seconds=1)),
                # )

                return result / timedelta(milliseconds=1)

            msg = {
                "timestamp": _timestamp_from_datetime(data),
                "measurements": [
                    (
                        data["m_systolic"],
                        data["m_diastolic"],
                    )
                ],
            }
            # finally send via mqtt
            mqtt_publish(
                mqtt_instance,
                get_mqtt_topic_from_config(config, device, ""),
                msg,
                log,
            )

        except Exception as e:
            log.debug(
                "{} Error while parsing bpm:{}\n {}".format(
                    h_time(), str(device), str(e)
                )
            )
    if data and (device.deviceType == "microhub"):
        if "battery" in data:
            try:
                log.debug("%s battery: %s", device.getName(), data["battery"])
                b_msg = {
                    "timestamp": round(sys_time.time() * 1000),
                    "timestep": 1,
                    "measurements": [data["battery"]],
                }
                mqtt_publish(
                    mqtt_instance,
                    get_mqtt_topic_from_config(config, device, "battery"),
                    b_msg,
                    log,
                )
            except Exception as e:
                log.debug(
                    "{} Error while receiving battery update?:{}\n {}".format(
                        h_time(), str(device), str(e)
                    )
                )
        if "data" in data:
            samples = data["data"]
            if len(samples) > 0:
                first_timestamp = samples[0]["t"]
                last_timestamp = samples[-1]["t"]
                timestep = round(last_timestamp - first_timestamp, 3)
                log.debug(
                    "{} [D]{}\t{:.2f}\tl\t{}\t{}".format(
                        h_time(),
                        device.name.ljust(20, " ")[-20:],
                        last_timestamp * 1000,
                        len(samples),
                        timestep,
                    )
                )

                # use this lambda to format output of samples
                def fmt_sample(f):
                    return round(f, 3)  # round to 3 decimal places

                # create mqtt msg
                acc_json, gyr_json = [], []
                for e in samples:
                    acc_sample, gyr_sample = [], []
                    # bed sensor only has acc
                    if device.microHubType == MicroHubType.BED:
                        acc_sample = e["s"]
                    else:
                        acc_sample = e["s"][0]
                        gyr_sample = e["s"][1]

                    if isnan(acc_sample[0]):
                        acc_json.append({})
                    else:
                        acc_json.append(
                            {
                                "x": fmt_sample(acc_sample[0]),
                                "y": fmt_sample(acc_sample[1]),
                                "z": fmt_sample(acc_sample[2]),
                            }
                        )

                    if len(gyr_sample) > 0:
                        if isnan(gyr_sample[0]):
                            gyr_json.append({})
                        else:
                            gyr_json.append(
                                {
                                    "x": fmt_sample(gyr_sample[0]),
                                    "y": fmt_sample(gyr_sample[1]),
                                    "z": fmt_sample(gyr_sample[2]),
                                }
                            )

                acc_msg = {
                    "timestamp": round(last_timestamp * 1000),
                    "timestep": timestep,
                    "measurements": acc_json,
                }
                mqtt_publish(
                    mqtt_instance,
                    get_mqtt_topic_from_config(config, device, "accel"),
                    acc_msg,
                    log,
                )

                if device.microHubType != MicroHubType.BED:
                    gyr_msg = {
                        "timestamp": round(last_timestamp * 1000),
                        "timestep": timestep,
                        "measurements": gyr_json,
                    }
                    mqtt_publish(
                        mqtt_instance,
                        get_mqtt_topic_from_config(config, device, "gyro"),
                        gyr_msg,
                        log,
                    )


def runMainLoop(configuration, log):
    MAIN_LOOP_START_TIME = sys_time.time()
    _mqtt_client = mqtt_init(config, log)
    saam_queue = DeviceQueue()
    scanner = ScannerWorker(saam_queue, log)
    scanner.start()

    # add devices to scanned
    for device in configuration.devices:
        scanner.addMacToScan(device["mac"])

    # Infinite loop
    while True:
        msg = saam_queue.pop_blocking(1.0)
        # no msg received
        if (not msg) or (msg.msgType == MsgType.EmptyMessage):
            continue
        # scanner has found device not yet connected
        if msg.msgType == MsgType.ScanResult:
            for device in configuration.devices:
                if device["mac"] == msg.data:
                    startDeviceThread(device, saam_queue, log)
            continue
        # device has stopped it's thread, put it to scanner to get scanned back
        if (msg.msgType == MsgType.Stop) and msg.data:
            # log.debug("%s Device stopped: %s", h_time(), msg.sender)
            # if msg.sender and msg.sender.isAlive:
            #    try:
            #        msg.sender.stop()
            #    except Exception as exc:
            #        print(exc)
            scanner.addMacToScan(msg.sender.deviceMac)
            continue
        # handle device specific streams
        if msg.msgType == MsgType.DeviceSpecific and msg.sender:
            handleDeviceSpecificData(
                configuration, msg.sender, msg.data, _mqtt_client, log
            )
            continue
        # log device status changes
        if msg.msgType == MsgType.DeviceStatus:
            log.debug("%s Device status: %s", h_time(), msg.data)
            continue
        # log device errors
        if msg.msgType == MsgType.DeviceError:
            log.debug("%s Device error: %s", h_time(), msg.data)
            continue

        log.debug("%s Unexpected message: %s", h_time(), str(msg))

    mqtt_disconect(_mqtt_client)
    log.debug(
        "Main loop exiting! Ran for %s seconds!", sys_time.time() - MAIN_LOOP_START_TIME
    )


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("No config file path given as an argument!")
        sys.exit(0)
    if not os_is_file(sys.argv[1]):
        sys.exit(0)
    config = parse_saam_config_json(sys.argv[1])
    # check device id (if current id is path, use content of the file as location_id)
    # saam_location = config.location
    saam_location = "missing_location_file"
    if os_is_file(config.location):
        saam_location = open(config.location).readline().strip()
    config = config._replace(location=saam_location)

    log = get_logger(config.location)
    log.debug("%s Main loop started!", h_time())
    runMainLoop(config, log)
    sys.exit(1)
