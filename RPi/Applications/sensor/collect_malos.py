#!/usr/bin/python3

# import os
import zmq
import time
# import sys
import logging
# import threading

from collect import Collect

#
# MALOS DAQ
#
from matrix_io.proto.malos.v1 import driver_pb2 # MATRIX Protocol Buffer driver library
from matrix_io.proto.malos.v1 import sense_pb2  # MATRIX Protocol Buffer sensor library
from multiprocessing import Process, Manager, Value # Allow for multiple processes at once
from zmq.eventloop import ioloop, zmqstream     # Asynchronous events through ZMQ

from data_object import DataObject

# Handy function for connecting to the Error port
from utils import driver_keep_alive, register_data_callback, register_error_callback


#  2019-11-25: Single class for all MALOS sensors
#  2019-12-01: modified
class CollectMALOS(Collect):
   """ 
   Generic class for MALOS Data Acquisition
   """
   
   matrix_ip = '127.0.0.1' # Local device ip
   
   def __init__(self, config, queue, quantities=None):
      super().__init__(config, queue, quantities)
      self.unit = config['config']['sensor']
      self.args = (self.unit, self.matrix_ip, config['config']['base'])
      self.proc = dict()
      
      ioloop.install()                       # Initiate asynchronous events
      self.config_socket(config['config'])   # Configure DAQ sensor driver

      # Start Data, Ping, and Error Port connection
      self.proc['DATA']  = Process(target=register_data_callback, 
                                   args=(self.data_callback,)+self.args)
      self.proc['PING']  = Process(target=driver_keep_alive, args=self.args) 
      self.proc['ERROR'] = Process(target=register_error_callback, 
                                        args=(self.error_callback,)+self.args)
      for key, proc in self.proc.items():
         proc.name = self.unit + '_' + key.lower()
      # Print debug information
      logging.info("Connected to {}".format(config['config']['desc']))

   def config_socket(self, params):
      # Define zmq socket
      context = zmq.Context()
      socket = context.socket(zmq.PUSH)   # Create a Publisher socket
      url = 'tcp://{1}:{2}'.format(*self.args)
      socket.connect(url)        # Connect to configuration socket
      daq_config = driver_pb2.DriverConfig() # Create a new driver config
      daq_config.delay_between_updates = params['sample_time']  # Ts in sec
      daq_config.timeout_after_last_ping = params['keep_alive'] # DAQ keep-alive
      
      # Sensor calibration:
      #   Humidity sensor: Current temperature in Celsius for calibration
      if 'calibrate' in params:
         try:
            for quantity, value in params['calibrate'].items():
               calibrate = getattr(daq_config, params['driver'])
               setattr(calibrate, quantity, value)
         except:
            print("Exception in calibrate for {}".format(self.unit))
            pass
      # Send driver configuration through ZMQ socket
      socket.send(daq_config.SerializeToString())


   def error_callback(self, error):
      # Log error
      logging.error('{0}'.format(error))


   def data_callback(self, data):
      # Extract data
      timestamp_ms = int(time.time() * 1000)
      sensor_data = getattr(sense_pb2, self.unit)().FromString(data[0])
      super().send_data(timestamp_ms, sensor_data)


   def start(self, name=None):
      if name is None:
         name = self.proc.keys()
      elif isinstance(name, str):
         name = [name]
      for key in name:
         if key in self.proc:
            self.proc[key].start()
            
   def stop(self, name=None):
      if name is None:
         name = self.proc.keys()
      elif isinstance(name, str):
         name = [name]
      for key in name:
         if key in self.proc and self.proc[key].is_alive():
            logging.debug("Terminating {}".format(self.proc[key].name))
            self.proc[key].terminate()

   def join_all(self):
      logging.debug("Joining child processes [CollectMQTT]")
      for proc in self.proc.values():
         if proc and proc.is_alive(): 
            proc.join(self.TIMEOUT)
         logging.debug("Joined {}".format(proc))
         
    



