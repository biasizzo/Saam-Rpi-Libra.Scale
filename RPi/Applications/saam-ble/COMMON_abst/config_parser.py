import json
from collections import namedtuple
import sys, os


def parse_saam_config_json(pathConfigFile):
    def checkValues(values, id):
        if not hasattr(values, id):
            print("Config file has no ", id, " !")
            sys.exit(1)

    def checkDeviceValues(deviceDict, id):
        if id not in deviceDict:
            print("One of the devices has no attribute", id, " !")
            sys.exit(1)

    try:
        print("Current directory:{}".format(os.getcwd()))
        with open(pathConfigFile) as json_file:
            data = json.load(json_file)
            print("Config:\n", json.dumps(data, indent=4))
            values = namedtuple('Struct', data.keys())(*data.values())

            checkValues(values, "location")

            checkValues(values, "topic")
            checkValues(values, "devices")

            for dev in values.devices:
                checkDeviceValues(dev, "type")
                checkDeviceValues(dev, "location")
                checkDeviceValues(dev, "mac")

            checkValues(values, "mqtt_ip")
            checkValues(values, "mqtt_user")
            checkValues(values, "mqtt_pwd")
            checkValues(values, "mqtt_port")
            checkValues(values, "mqtt_timestep")
            checkValues(values, "mqtt_on")

            return values

    except Exception as e:
        print(e)
        raise e
