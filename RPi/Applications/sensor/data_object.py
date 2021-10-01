import logging

class ProcessContainer:
   """
   Complementary class to the DataObject that stores object processing functions.
   Each DataOnject class is associated with a measured quantity.
   For each measured quantity there are defined two sets: a set of 
   processing functions and a set of reporting functions. These sets
   are managed by ProcessContainer class.
   For each DataObject class there should be a single instance of 
   this class, since processing and reporting functins are the same
   for all class instances
   """
   def __init__(self, name):
      self.name = name
      self.functions = []
      self.reports = []

   def addFunction(self, function, queue):
      self.functions.append((function, queue))

   def addReport(self, function):
      self.reports.append(function)

   def process(self, obj): # call processing and reporting functions
      for function, queue in self.functions:  # Call processing functions
         try:                                 # Can be used for feature extraction
            function(obj, queue)
         except:
            logging.error("Exception calling processing function {} with object {}".format(function, obj))
            logging.error("   Obj class: {}".format(obj.__class__.__name__))
      for function in self.reports: # Call reporting functions
         try:                       # Used for transmitting data to cloud, database
            function(obj)
         except:
            logging.error("Exception calling report function {}".format(function))
            logging.error("   Obj class: {}".format(obj.__class__.__name__))





class DataObject:
   """
   Generic Data Object
     - data value (may be of a complex data type)
     - acquisition time
     - processing container (class ProcessContainer)
   """
   # print("Static part of DataObject")
   desc  = None  # List of value component descriptions e.g. ['_x', '_y', '_z']
   proc  = None  # Modified to class static variable because on Queue.get()
                 # a ProcessContainer construction with empty name is called
   # proc  = ProcessContainer(__qualname__)  # Called only at module initialization

   def __init__(self, time, value=None):
      self.time = time
      self.value = value

   def initProcessContainer(self):
      if self.__class__.proc: # Return if class's ProcessContainer 
         return               # is already created
      name = self.__class__.__name__
      self.__class__.proc = ProcessContainer(name) # Create ProcessContainer
      
   def setDescription(self, description):
      self.__class__.desc = description

   def getDescription(self):
      return self.__class__.desc

   def addFunction(self, function):
      self.proc.addFunction(function, self.queue)

   def addPackage(self, function):
      self.proc.addReport(function)

   def process(self):
      if self.proc:
         self.proc.process(self)

   def time(self, time=None):
      if time:
         self.time = time
      return self.time

   def get(self):
      return self.value

   def __repr__(self):
      out = "{} [@{}]: {}".format(self.__class__.__name__.capitalize(), self.time/1000, self.value)
      if self.desc:
         out = "{}  # {}".format(out, self.desc)
      return out
            
   def pkg(self): ### Probably the package should do this (lower footprint)
      """
      Convert values (array) into dictionary if descriptions are available
      """
      if self.desc:
         return dict(zip(self.desc, self.value))
      return self.value


