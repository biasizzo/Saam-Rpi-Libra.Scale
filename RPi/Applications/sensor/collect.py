#!/usr/bin/python3

# import os
# import time
# import sys
# import logging

from data_object import DataObject

# Added: 2019-12-01: Generis class for all DAQ
class Filter:
   """
   Input filtering of DAQ quantities
   """
   def __init__(self, zero, size, function):
      self.size   = size           # Size of internal buffer
      self.cnt    = 0              # Current pointer to free slot in the buffer
      self.zero   = zero
      self.value  = zero
      self.full   = False          # Is buffer full (affects initial settings)
      self.avg    = function.startswith('avg_')
      try:
         self.function = getattr(self, function)     # filter function
      except:
         self.function = None
      # if function in use_buffer:
      if function.endswith('_slide'): # buffer is needed for sliding window
         self.array  = [zero] * size  # Buffer (zero is an element of a buffer)
      else:
         self.array  = None


   def avg_sample(self, value):
      if self.cnt == 0:
         self.value = value
         self.cnt = 1
      else:                       #  n-1 = self.cnt 
         self.value *= self.cnt   #  get X_1 + X_2 + ... + X_n-1
         self.value += value      #  get X_1 + X_2 + ... + X_n-1 + X_n
         self.cnt   += 1
         self.value /= self.cnt   #  new average
         self.cnt   %= self.size  #  wrap 
      
   def avg_slide(self, value):
      if self.full:
         self.value += (value - self.array[self.cnt]) / self.size
         self.array[self.cnt] = value
         self.cnt   += 1
         self.cnt   %= self.size  #  wrap 
      else:
         self.array[self.cnt] = value
         self.avg_sample(value)
      
   def min_sample(self, value):
      if self.cnt == 0:
         self.value = value
      elif self.value > value:
         self.value = value
         
   def min_slide(self, value):
      self.value = value
      for val in self.array:
         if val < self.value:
            self.value = val
      
   def max_sample(self, value):
      if self.cnt == 0:
         self.value = value
      elif self.value < value:
         self.value = value
         
   def max_slide(self, value):
      self.value = value
      for val in self.array:
         if val > self.value:
            self.value = val
      
   
   def process(self, value):
      # Correct way would be to check time difference for possible
      # missed readout and skip 
      if self.avg and self.function:
         self.function(value)
      else:
         if self.array:
            self.array[self.cnt] = value  # store current value in the buffer
         if self.function:
            self.function(value)
         else:
            self.value = value
         self.cnt   += 1                  # increment circular buffer pointer
         self.cnt   %= self.size          # wrap around in buffer
      if self.cnt == 0:
         self.full = True                 # buffer is full from now on
         return self.value
      elif self.array:                    # resampled since no sliding window
         return self.value                   # sliding window
      return None



# Added: 2019-12-01: Generis class for all DAQ
class Collect:
   """
   Generic class for Data Acquisition
   """
   TIMEOUT = None
   
   def __init__(self, config, queue, quantities=None):
      """
      DAQ Initialization:
        - config:  DAQ configuration description
        - queue:   Queue to which data objects created by acquired data are passed
        - filter:  Filter used for data preprocessing ???
      """
      self.queue = queue              # Store output queue
      self.quantity = dict()          # Store configuration for quantity
      for quantity, channel in config['quantity'].items():
         if not quantities or quantity not in quantities:
            continue
         size = 0
         daq_filter = None            # Initially without filter
         name = quantity.capitalize() + '_class'
         data_class = type(name, (DataObject,), {})  # Create new class with name
         globals()[name] = data_class                # register new class to module
         if isinstance(channel, tuple):              # tuple indicates filter
            size = channel[2];                       # Number of samples for filter
            function = channel[1]                    # Filter function
            channel = channel[0]                     # DAQ channels
         if isinstance(channel, str):
            if size:                    # size > 0 indicates filter
               daq_filter = Filter(0, size, function)
            self.quantity[quantity] = ({channel: daq_filter}, data_class)
         elif isinstance(channel, dict):
            desc = list(channel.keys())
            if size:                    # size > 0 indicates filter
               daq_filter = [ Filter(0, size, function) for key in desc ]
            else:
               daq_filter = [daq_filter] * len(desc)
            data_class.desc = desc
            channel = dict(zip(channel.values(), daq_filter))
            self.quantity[quantity] = (channel, data_class)
      self.config = config['quantity']  # Store quantity descriptions REDUNDANT!!!
      
      
   def send_data(self, timestamp, data):
      for qid, (channel, data_class) in self.quantity.items():
         record = []
         empty = True
         try:
            for name, filter in channel.items():
               value = getattr(data, name)
               value0 = value
               if filter: #  Apply filter to input data
                  value = filter.process(value)
               record.append(value)
               empty = empty and value is None
         except:
            continue
         if empty:  # Skip empty DAQ data
            continue
         if len(record) == 1:
            record = record[0]
         data_object = data_class(timestamp, record)
         self.queue.put(data_object)


