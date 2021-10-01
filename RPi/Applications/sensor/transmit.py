import logging
import threading
from multiprocessing import Queue
from settings import run
import zerorpc
import paho.mqtt.client as mqtt
import ssl

class Transmit:
   def __init__(self, name):
      self.name  = name
      # self.queue = Queue()

   def get_name(self):
      return self.name

   def get_queue(self):
      return self.queue

   def send(self, package):
      pass


class TransmitMQTT(Transmit):
   """
   Transmission using MQTT protocol
   """
   def __init__(self, name, config):
      super().__init__(name)
      self.queue = Queue()
      # init MQTT client
      self.root_topic = ""
      ### DEBUG setting from configuration
      self.debug = False
      if 'DEBUG' in config:
        self.debug = config['DEBUG']
      ####################################
      if 'TOPIC' in config:
         self.root_topic = config['TOPIC']
      if self.root_topic and not self.root_topic.endswith("/"):
         self.root_topic += "/"
      self.mqtt = mqtt.Client()
      self.mqtt.username_pw_set(username=config['USER'], password=config['PASS'])
      self.mqtt.on_connect = self.on_connect
      self.mqtt.on_message = self.on_message

      # TSL/SSL ...
      if 'SSL' in config:
          self.mqtt.tls_set(ca_certs=config['SSL']['CA'],
                            certfile=config['SSL']['CERT'],
                            keyfile =config['SSL']['KEY'],
                            cert_reqs=ssl.CERT_NONE, ciphers=None)
          self.mqtt.tls_insecure_set(True)
      connect_mqtt = self.mqtt.connect(config['HOST'], 
                                       config['PORT'],
                                       config['KEEPALIVE'])
      if connect_mqtt == 0:
          logging.info("Connected to MQTT address: {}".format(config['HOST']))
      else:
          logging.error("Error connecting to MQTT address: {}".format(config['HOST']))
      self.mqtt.loop_start()  # start loop // Added 2019-03-14
      # process
      self.thread = threading.Thread(target=self.process)
      self.thread.start()

   def process(self):
      logging.debug("Transmit queue: {}".format(self.queue))
      try:
         while run['PROCESS']:
            data = self.queue.get()
            logging.debug("Transmit - get data: {}".format(data))
            if data is None:
               continue
            topic = self.root_topic + run['LOCATION_ID'] + "/" + data.id
            data = data.generate_json()
            if self.mqtt.publish(topic, data):
               logging.info("MQTT topic {} successfully sent".format(topic))
            else:
               logging.error("MQTT topic {} failed".format(topic))
                    
            if self.debug:
               logging.debug("Send data: {}".format(data))

      except KeyboardInterrupt:
         # raise
         logging.warning('MQTT [{}] KeyboardInterrupt'.format(self.name))
         self.mqtt.loop_stop()  # stop loop // Added 2019-03-14
         self.mqtt.disconnect()

   # The callback for when the client receives a CONNACK response from the server.
   def on_connect(self, client, userdata, flags, rc):
      logging.info("Connected flags {} result code {}".format(flags, rc))

   # The callback for when a PUBLISH message is received from the server.
   def on_message(self, client, userdata, msg):
      logging.info("{} {}".format(msg.topic, msg.payload))

   def join(self):
      if self.thread.is_alive():
         self.queue.put(None)
      self.thread.join()
      logging.debug("Transmission channel {} joined".format(self))


