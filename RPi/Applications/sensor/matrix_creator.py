#!/usr/bin/python3 -u

import os, sys, time, signal
import logging, logging.handlers
from multiprocessing import Queue
from setproctitle import setproctitle

# if __name__ == '__main__':
# Change current working directory to executable folder
pathList = sys.argv[0].split("/")
del pathList[-1]
os.chdir("/".join(pathList))

import collect
from collect_malos import CollectMALOS
from collect_audio import CollectAudio

import transmit
from package import Package
import settings as stg

TERMINATE = [signal.SIGTERM, signal.SIGINT, signal.SIGQUIT, signal.SIGABRT]
oldHnd = dict()

from subprocess import Popen, PIPE

def command(log, cmd, desc='-------------'):
    log.info(desc)
    proc = Popen(cmd.split(), stdout=PIPE, stderr=PIPE)
    lines = proc.stdout.readlines()
    for line in lines:
        log.info(line.strip().decode('utf-8'))
    proc.wait()
    proc.stdout.close()
    proc.stderr.close()
    log.info('-------------')


def config_reload(sig, frame):  # Signal handler to reload configuration
    logging.debug("Process {} received signal {}".format(os.getpid(), sig)) 
    try:                            # Actually just location_id
        locfile = open('/boot/SAAM/location.id', 'r')
        line = locfile.readline().strip()
        locfile.close()
        if line:
            stg.run['LOCATION_ID'] = line
    except:
        pass
   
def stop_matrix(sig, frame):    # Signal handler to stop acquisition
    logging.debug("Process {} received signal {}".format(os.getpid(), sig)) 
    stg.run['PROCESS'] = False
    signal.signal(sig, oldHnd[sig])
    if oldHnd[sig]:
       oldHnd[sig](sig, frame)
   

def setLogging():
    root = logging.getLogger()
    level = getattr(logging, stg.log_parameters['LEVEL'].upper(), None)
    format = None
    if 'FORMAT' in stg.log_parameters.keys():
        datefmt = None
        if 'DATE' in stg.log_parameters.keys():
            datefmt = stg.log_parameters['DATE']
        format = logging.Formatter(fmt=stg.log_parameters['FORMAT'], datefmt=datefmt)
    if 'FILE' in stg.log_parameters.keys():
        file = stg.log_parameters['FILE']
        # Remove old logging handlers (to stderr)
        for hnd in root.handlers:
            root.removeHandler(hnd)
        if file:
            # logging to file
            file = file.format('ambient.sensor')
            size = 0
            if 'SIZE' in stg.log_parameters:
                size = stg.log_parameters['SIZE']
            size = size * 1024 * 1024 # Megabytes
            cnt  = 0
            if 'CNT' in stg.log_parameters:
                cnt = stg.log_parameters['CNT']
            hnd = logging.handlers.RotatingFileHandler(file, maxBytes=size, backupCount=cnt)
            root.addHandler(hnd)
        else:
            # use syslog
            facility = logging.handlers.SysLogHandler.LOG_LOCAL1
            root.addHandler(logging.handlers.SysLogHandler(address="/dev/log", facility=facility))

    # leave default logging handler (console)
    root.setLevel(level)
    if format:
        for hnd in root.handlers:
            hnd.setFormatter(format)

if __name__ == '__main__':
    setproctitle('Ambient')
    # Initialize logging
    setLogging()
    signal.signal(signal.SIGHUP, config_reload)
    # Initialise queues
    queue = Queue() # Main queue which receives objects with acquired data 
                    # along with process and report functions from sensors

    # Filter quantities with known transmit channel
    ch_set = set(stg.transmit.keys())
    get_list = lambda x: x if isinstance(x, list) else [x]
    quantity = {key for key, dest in stg.report.items() if ch_set.intersection(get_list(dest[3]))}
    # Get channels for quantities
    channels = ch_set.intersection({name for dest in stg.report.values() for name in get_list(dest[3])})

    logging.debug("Quantities: {}".format(quantity)) 
    logging.debug("Channels: {}".format(channels)) 

    """
    Start Data Acquisition to a queue
    """
    sensors = []
    for key, config in stg.sensors.items():
       if not config: # Skip if empty config
          continue
       quant = quantity.intersection({q for q in config['quantity']})
       if not quant : # Skip if empty quantity set for current sensor
          continue
       logging.debug("Sensors quantities: {}".format(quant)) 
       try:
          # daq_class = getattr(collect, 'Collect' + config['type'])
          daq_class = globals()['Collect' + config['type']]
          # daq = daq_class(config, queue)
          daq = daq_class(config, queue, quant)
          if config['type'] == 'MALOS':
             daq.start(['DATA', 'ERROR'])  # start only data and error listener; PING started later
          sensors.append(daq)
          
       except:
          logging.error("Sensor {} failed".format(key))
          pass

    """
    Start Transmission modules
    They are executed in separate threads
    """
    transmission = dict()
    for name, config in stg.transmit.items():
       if name not in channels:
          continue
       try:
          transmit_class = getattr(transmit, 'Transmit' + config['TYPE'])
          transmission[name] = transmit_class(name, config)
       except:
          logging.error("Transmit {} failed".format(name))
          # result = transmit.pop(name, None)  # remove 'name' element
          pass
    
    stg.run['PROCESS'] = transmission and sensors
    if stg.run['PROCESS']:  
       """
       Register Packaging and Processing functions
       All are executed in this thread (in the while loop after instantiation)
       """
       for name, config in stg.report.items():  # for quantities in report list
          name = name.capitalize() +  '_class'  # get the quantity data class
          try:
            data_class = getattr(collect, name)
          except:
             continue
          # 2020.03.04: handling multiple transmit channels
          channels = config[3]
          if not isinstance(channels, list):
             channels = [channels]
          pkg_queue = [transmission[chn].get_queue() for chn in channels if chn in transmission]
          if not pkg_queue:  # If transmission not available skip ...
             continue
          if len(pkg_queue) == 1: # If single transmission channel do not use list
             pkg_queue = pkg_queue[0]
          dummy = data_class(0,0)    # Create dummy object, to set global settings
          package = Package(config, pkg_queue, dummy.getDescription()) # create 
          dummy.initProcessContainer()
          dummy.addPackage(package.append)
          
       for sensor in sensors: # Start sensor acquisition (by start pinging)
          sensor.start('PING')

    for sig in TERMINATE:
       oldHnd[sig] = signal.getsignal(sig)
       signal.signal(sig, stop_matrix)
       
    while stg.run['PROCESS']: # Main loop: pick objects from queue and process them
       try:
          data = queue.get()
          logging.debug("Data: {}".format(data))
          data.process()
       except KeyboardInterrupt: # Stop on any error while processing
          stg.run['PROCESS'] = False

    logging.info("Acquisition loop stopped")

    # Added: 2019-11-22
    # Stop the processing
    try:
        logging.debug("Stopping Acquisition processes")
        for sensor in sensors:
           sensor.stop()        # Stop acquisition processes, and
           sensor.join_all()    # wait for their termination
        logging.error("Sensors joined") 
        for transmitting in transmission.values():
           transmitting.join()  # Wait for transmission module termination
        logging.error("Transmittors joined") 
    except (KeyboardInterrupt, SystemExit):
        logging.error("Caught the exception") 
        for sensor in sensors:
           sensor.stop()        # Stop acquisition processes, and
           sensor.join_all()    # wait for their termination
        for transmitting in transmission.values():
           transmitting.join()  # Wait for transmission module termination

