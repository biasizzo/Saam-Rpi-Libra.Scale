#!/usr/bin/python3

import logging
from threading import Lock
import copy
import json
from settings import run

########################################################
# Rounding numeric elements in data objects to 
# reduce data load
# Added: 2019-03-21, 2019-11-22
########################################################

########################################################

class Package:
   """
   Wrapping class for packages that in addition stores queue and lock

   Current implementation does not use alarms for packaging (more accurate)
   but locking mehanizem is still in place.
   """
   def __init__(self, config, send_queue, description=None):
      self.pkg   = PackageData(config, description)
      self.lock  = Lock()       # Lock for multithread implementation (alarms)
      self.send  = send_queue   # send queue
      

   def append(self, data_object):
      self.lock.acquire()         # Lock the object
      self.pkg.append(data_object.time, data_object.value, self.send)
      self.lock.release()         # Release the lock on object

   def alarm(self):        # callback function for sending data by alarm
      self.lock.acquire()  # lock the object
      try:
         if self.pkg.data:
            # self.send(self.generate_json())
            self.send.put(self.pkg)
         self.pkg.clear()
      finally:
         self.lock.release()


class PackageData:
   """
   Class that packages samples over reporting period.
   Package is then rounded to given precision and sent to send_queue
   """
   def __init__(self, config, description=None):
      self.id     = config[0]        # source_id    # source_id of data (e.g. for MQTT)
      self.period = config[1] * 1000 # period       # Reporting period
      self.prec   = config[2]        # precision    # Precision of the data
      self.desc   = description      # Description to package arrays
      # logging.debug("Package description[{}]: {}".format(self.id, description))
      self.clear()


   def clear(self):
      self.start  = None         # There are no samples
      self.time   = None
      self.data   = []           # Clear the data list

   def copy(self):
      pkg = copy.copy(self)
      pkg.data = copy.copy(pkg.data)
      return pkg
       
   def append(self, time, data, send): # With and without alarms for sending
      if self.start is not None:
         if time - self.start < self.period:
            # self.data.append(data)                   # Not rounded data stored
            self.data.append(self.round_data(data, self.prec))  # Store rounded data
            self.time = time
            return
      if self.data:  # Do not send if there is no data
         # self.send(self.generate_json())   # Directly sending
         # print("Enqueue copy of {}: {}".format(self, self.__dict__))
         # 2020-03-04 multiple destions using multiple queues in a list
         if isinstance(send, list):
            for queue in send:
               queue.put(self.copy())
         else:
            send.put(self.copy())
      self.start = time
      self.time  = time
      self.data  = [self.round_data(data, self.prec)]
      # Added for event processing (period = 0)
      if self.period == 0:
         if isinstance(send, list):
            for queue in send:
               queue.put(self.copy())
         else:
            send.put(self.copy())
         self.clear()



   def round_data(self, data, prec):
       if isinstance(data, float):
           if not isinstance(prec, int):  # precision of single value must be int
               return data
           data = round(data, prec)
           if prec <= 0:
               data = int(data)
           return data
       precision = lambda k: prec.get(k, 2) if isinstance(prec, dict) else prec
       if isinstance(data, list):
           return [self.round_data(value, precision(key)) for key, value in zip(self.desc, data)]
       if isinstance(data, dict):
           # returns key component of prec if exists, otherwise it returns prec
           return {key: self.round_data(value, precision(key)) for key, value in data.items()}
       return data

   def get_start_time(self):
       return self.start_time


   def pack(self):  # Pack only required components
      msg = dict()
      msg['start'] = self.start
      msg['time']  = self.time
      msg['id']    = self.id
      msg['data']  = self.data
      msg['desc']  = self.desc
      return msg

   def generate_json(self):  # Move to transmit part
      msg = dict()
      msg['timestamp']    = self.time
      msg['timestep']     = (self.time - self.start) / 1000
      msg['location_id']  = run['LOCATION_ID']
      msg['source_id']    = self.id
      msg['measurements'] = self.data  # data rounded at append
      if self.desc:
         msg['measurements'] = [dict(zip(self.desc, value)) for value in self.data]
         
      return json.dumps(msg)
