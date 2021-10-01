import json
import time
import datetime
import pytz
import atexit

import paho.mqtt.client as mqtt
import ssl

from settings import transmit, run # default values 

class MQTT:
    # The callback for when the client receives a CONNACK response from the server.
    def on_connect(client, userdata, flags, rc):
        #print("Connection flags ", str(flags), "result code ", str(rc))
        pass
    
    # The callback for when a PUBLISH message is received from the server.
    def on_message(client, userdata, msg):
        #print(msg.topic + " " + str(msg.payload))
        pass

    def __init__(self, mqtt_parameters=None, debug=False):
        if mqtt_parameters is None: 
            mqtt_parameters = transmit['mqtt']

        self.debug = debug
        self.client_mqtt = mqtt.Client()
        self.client_mqtt.username_pw_set(username=mqtt_parameters['USER'], password=mqtt_parameters['PASS'])
        self.client_mqtt.on_connect = MQTT.on_connect
        self.client_mqtt.on_message = MQTT.on_message
        if 'SSL' in mqtt_parameters:
            self.client_mqtt.tls_set(ca_certs=mqtt_parameters['SSL']['CA'],
                                     certfile=mqtt_parameters['SSL']['CERT'],
                                     keyfile=mqtt_parameters['SSL']['KEY'],
                                     cert_reqs=ssl.CERT_NONE, ciphers=None)
            self.client_mqtt.tls_insecure_set(True)
        connect_mqtt = self.client_mqtt.connect(mqtt_parameters['HOST'], 
                                                mqtt_parameters['PORT'],
                                                mqtt_parameters['KEEPALIVE'])
        self.client_mqtt.loop_start()
        if self.debug:
            if connect_mqtt == 0:
                print('Data will be transmitted via MQTT')
                print("Connected to MQTT address:", mqtt_parameters['HOST'])
            else:
                print("Error connecting to MQTT address:", mqtt_parameters['HOST'])
        atexit.register(self.on_exit)
    
    def on_exit(self):
        self.client_mqtt.loop_stop()
        self.client_mqtt.disconnect()
        if self.debug:
            print('MQTT loop finished, client disconnected')
        
    # Modified: 2019.09.14
    # def send_to_topic(self, source_id, location_id, topic_prefix='saam/data', duration=0, measurements=[0]):
    def send_to_topic(self, source_id, topic_prefix='saam/data', timestep=0, measurements=[0]):
        timestamp = int(time.time() * 1000)
        data = {
            'timestamp': int(timestamp),
            'source_id': source_id,
            'location_id': run['LOCATION_ID'],
            'timestep': timestep,
            'measurements': measurements
        }
        
        topic = '{}/{}/{}'.format(topic_prefix, run['LOCATION_ID'], source_id)
        message_info = self.client_mqtt.publish(topic, payload=json.dumps(data))
        if self.debug:
            if message_info.rc == mqtt.MQTT_ERR_SUCCESS:
                print('data sent, message ID: {}'.format(message_info.mid))
                print('data:\n{}'.format(json.dumps(data)))
            elif message_info.rc == mqtt.MQTT_ERR_NO_CONN:
                print('error: no connection')
            elif message_info.rc == mqtt.MQTT_ERR_QUEUE_SIZE:
                print('error: queue full')
            else:
                print('error: return code {}'.format(message_info.rc))
        return message_info.rc


