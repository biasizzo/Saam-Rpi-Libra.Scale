#!/usr/bin/python3

import sys, os, time, ssl, signal
import logging, logging.handlers
from multiprocessing import Queue, Lock
from persistqueue import Queue as PQueue
import subprocess
from datetime import datetime
from dateutil.parser import parse
from apscheduler.schedulers.background import BackgroundScheduler

import paho.mqtt.client as mqtt
import argparse
import json
from setproctitle import setproctitle
from importlib import reload  

import settings as stg

# Constant
_mqtt_keys = ['language', 'pipeline_name', 'coaching_action', 'variant'] # Action keys from MQTT
_required_mqtt_keys = ['language', 'pipeline_name', 'coaching_action']   # Required keys from MQTT
# Changes for adopting Yavor ...
# _mqtt_keys = ['Language', 'PiplineName', 'CoachingAction', 'Variant'] # Action keys from MQTT
# _required_mqtt_keys = ['Language', 'PiplineName', 'CoachingAction']   # Required keys from MQTT
_domain={'sleep':    1,  # pipeline_name ==> domain lookup table
         'social':   2,
         'mobility': 3,
         'activity': 4}
_trig_time_args = ['hour', 'minute', 'second']  # Configurable trigger time attributes
# RENDER_QUEUE_PATH = "/run/user/{}/Coaching.Actions" # {} is placehold for uid 
RENDER_QUEUE_PATH = "Coaching.Actions"

# Global variables - could be moved to objects in OO
cmd_queue = None       # Command queue
_render_queue = None   # Queue for rendering actions
lock_queue = None      # Queue lock object
sched_jobs = []        # Active scheduler jobs
last_valid = 0

# DELAY    = 2
# MESSAGES = "message.store"
# PERIOD   = 86400

'''
Signals for handling Render module:
   - TERMINATE: Rendering module is terminated.
                A STOP message is pushed to the message queue, 
                which in turn terminates the process
   - REPLAY:    Messages pickled and stored in a file "MESSAGES" are
                enqueued in message queue, if they are not too old.
                Message file "MESSAGES" is deleted or emptied
   - ERASE:     Message file "MESSAGES" is deleted or emptied
   - RELOAD:    The RELOAD message is pushed to the message queue.
                Program loop is restarted and parameters reloaded.
'''

SIGNALS = dict(
  TERMINATE = [signal.SIGTERM, signal.SIGINT, signal.SIGQUIT, signal.SIGABRT],
  REPLAY    = [signal.SIGALRM, signal.SIGUSR2],
  ERASE     = [signal.SIGUSR1],
  RELOAD    = [signal.SIGHUP]
)

def signal_handler(signalNumber, frame):  # Signal handler to reload configuration
   logging.debug("Got signal {}".format(signalNumber))
   if signalNumber in SIGNALS['TERMINATE']: # Terminate 
      logging.info("Terminate")
      stg.run['PROCESS'] = False;
      cmd_queue.put('STOP')
      return
   # if signalNumber in SIGNALS['REPLAY']:    # Replay messages
   #    oldest = time.time() - PERIOD         # At most one day old messages
   #    lock_queue.acquire()
   #    try:
   #       with open(MESSAGES, "rb") as messages:
   #          for msg in pickle.load(messages):
   #             if msg['time'] > oldest:
   #                _render_queue.put(msg['item'])
   #       os.remove(MESSAGES)
   #    except:
   #       pass
   #    lock_queue.release()
   #    return
   if signalNumber in SIGNALS['RELOAD']:    # Reload configuration
      logging.info("Reload configuration")
      try:                              # Actually just location_id
         locfile = open('/boot/SAAM/location.id', 'r')
         line = locfile.readline().strip()
         locfile.close()
         if line:
            stg.run['LOCATION_ID'] = line
      except:
         pass
      cmd_queue.put('RELOAD')
      return
   

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
            file = file.format('render')
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
            


def my_on_connect(client, userdata, flags, rc):
   if rc:
     logging.debug("Connected flags ", str(flags), "result code ", str(rc))
     sys.exit(-1)



def my_on_message(client, userdata, msg):
   timestamp = int(time.time())
   try:
      data = json.loads(msg.payload)
   except:
      logging.warning("MQTT: Error parsing json: {}".format(msg.payload))
      return
   logging.debug("MQTT: {}: {}".format(msg.topic, data))
   action = {key: data[key] for key in _mqtt_keys if data[key] is not None} # Filter out dict keys
   missing = [x for x in _required_mqtt_keys if x not in action]
   # if len(action) != len(_mqtt_keys):
   if missing:
      logging.warning("Missing required rendering parameters: {}".format(missing))
      return
   if not _render_queue:
      return
   action['timestamp'] = timestamp
   lock_queue.acquire()
   # Local coaching action: load from storage (deprecated)
   # msgs = []
   # oldest = timestamp - PERIOD
   # try:
   #    messages = open(MESSAGES, "rb")
   #    msgs = [msg for msg in pickle.load(messages) if msg['time'] > oldest]
   #    messages.close()
   # except:
   #    pass
   # msgs.append({'time': timestamp, 'item': action})
   try:
      _render_queue.put(action)
      logging.info("Put into queue: {}".format(action))
   except e:
      logging.error("Error enqueueing action: {}".format(missing))
   # Local coaching action: store appended actions for post processing (deprecated)
   # try:
   #    messages = open(MESSAGES, "wb")
   #    pickle.dump(msgs, messages)
   #    messages.close()
   # except:
   #    pass
   lock_queue.release()



def do_rendering():
   global last_valid
   item_read = False
   while not _render_queue.empty():
      msg = _render_queue.get()
      item_read = True
      if not stg.render['enabled']:
         continue
      if last_valid and msg['timestamp'] < last_valid:  # Skip too old coaching actions
         continue
      # if 'stop' in msg:
      #    stg.run['PROCESS'] = False
      #    break
      # if 'reload' in msg:
      #    break
      # Not needed since done in ..._on_message function
      ###################################################################
      # missing = [x for x in _required_mqtt_keys if x not in msg]
      # if missing:   # Ignore messages that are not mqtt render messages
      #    continue
      ###################################################################
      action = [msg[x] for x in _mqtt_keys if x in msg] # build ordered array
      domain = action[1].split('_')[0]
      filename = "-".join(action)
      audioname = "{}/{}/{}.ogg".format(action[0], domain, filename)
      if not os.path.isfile(audioname):
         logging.info("Audio file {} for coaching does not exist".format(audioname))
         continue
      # try:
      #    domain = _domain[action[1].split('_')[0]]
      # except:
      if domain not in _domain:
         continue
      # cmd = ['./render', str(domain), action[0], filename]
      cmd = ['./render', str(_domain[domain]), action[0], filename]
      # cmd.append(filename)
      logging.info("Render: {}  {}".format(domain, filename))
      logging.debug("Command: {}".format(cmd))
      _render = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
      _render.wait()
      if not _render_queue.empty():
         time.sleep(stg.render['delay'])
   if item_read:
      _render_queue.task_done()


def configure(args):
   # Default settings if not given in configuration file (settings.py)
   topic = 'saam/messages'
   mqtt_name = 'mqtt'
   if args.source:
      mqtt_name = args.source
   if args.topic:
      topic = args.topic
   default = dict()
   if mqtt_name not in stg.transmit.keys():   # Default source
      default[mqtt_name] = [topic]
   sources = dict()                           # Source list from settings.py
   if 'render' in stg.__dict__:
      if 'channel' in stg.render:
         chnls = stg.channels[stg.render['channel']]
         if not isinstance(chnls, list):
            chnls = [chnls]
      else:
         chnls = []
         logging.warning("Input channel {} is not in the configuration".format(stg.render['channel']))
      for (topics, names) in chnls:
         if not isinstance(names, list):
            names = [names]
         for mqtt_name in names:
            if mqtt_name not in stg.transmit.keys():
               logging.warning("MQTT broker {} is not in the configuration".format(mqtt_name))
               continue
            if mqtt_name not in sources:
               sources[mqtt_name] = []
            if isinstance(topics, list):
               sources[mqtt_name] += topics
            else:
               sources[mqtt_name].append(topics)
               
   if not sources:
      return default
   return sources

def mqtt_initialize(config):
   MQTT_ID = "{}_{}".format(stg.run['LOCATION_ID'], os.getpid())
   mqtts = dict()
   for mqtt_name in config:
      mqtt_cfg = stg.transmit[mqtt_name]
      try:
         client = mqtt.Client(MQTT_ID, False);
         client.username_pw_set(username=mqtt_cfg['USER'], password=mqtt_cfg['PASS'])
         client.on_connect = my_on_connect
         client.on_message = my_on_message
        
         # TSL/SSL ...
         if 'SSL' in mqtt_cfg:
            client.tls_set(ca_certs=mqtt_cfg['SSL']['CA'],
                           certfile=mqtt_cfg['SSL']['CERT'],
                           keyfile =mqtt_cfg['SSL']['KEY'],
                           cert_reqs=ssl.CERT_NONE, ciphers=None)
            client.tls_insecure_set(True)
         connect = client.connect(mqtt_cfg['HOST'],
                                  mqtt_cfg['PORT'],
                                  mqtt_cfg['KEEPALIVE'])
     
         mqtts[mqtt_name] = client
      except:
         pass
   return mqtts

def mqtt_subscribe_and_start(mqtts, config):
   if not mqtts or not config:
      return
   for name, client in mqtts.items():
      for root_topic in config[name]:
         topic = "{}/{}/#".format(root_topic, stg.run['LOCATION_ID'])
         logging.info("Subscribe on {} to topic: {}".format(stg.transmit[name]['HOST'], topic))
         client.subscribe(topic, 2)
      client.loop_start()

def mqtt_unsubscribe_and_stop(mqtts, config):
   if not mqtts or not config:
      return
   for name, client in mqtts.items():
      for root_topic in config[name]:
         topic = "{}/{}/#".format(root_topic, stg.run['LOCATION_ID'])
         logging.info("Unsubscribe on {} to topic: {}".format(stg.transmit[name]['HOST'], topic))
         client.unsubscribe(topic)
      client.loop_stop()

def trigger_rendering():
   cmd_queue.put('PLAY')

def initialize_rendering(schedule, args, reload=False):
   global last_valid
   last_valid = 0
   if 'history' in stg.render:
      last_valid = time.time() - stg.render['history']
   config = configure(args)
   if not config:
      logging.error("No valid render configurations")
      # stg.run['PROCESS'] = False
      return None, None
   # while not _render_queue.empty():  # Empty the queue
   #   _render_queue.get()
   mqtts = mqtt_initialize(config)
   if not mqtts:
      logging.error("None MQTT broker were initialized")
      stg.run['PROCESS'] = False
      return None, None
   #### Setup events!!!
   mqtt_subscribe_and_start(mqtts, config)
   logging.debug("Scheduler periods: {}".format(stg.render['periods']))
   for rtime in stg.render['periods']:
      ev_time = None
      try:
         rdt = parse(rtime)
         ev_time = {key:getattr(rdt, key) for key in _trig_time_args}
      except:
         logging.debug("Scheduler rtime: {}".format(ev_time))
         try:
            ev_time = dict(zip(_trig_time_args, rtime.split(':')))
         except:
            pass
      logging.debug("Scheduler event: {}".format(ev_time))
      if not ev_time:
         continue
      try:
         ev_time = {key:val for key, val in ev_time.items() if val}
         job = schedule.add_job(trigger_rendering, 'cron', **ev_time)
         logging.debug("Schedule render at {}".format(ev_time))
         sched_jobs.append(job)
      except:
         logging.error("Error adding scheduler job at {}".format(rtime))
   return config, mqtts

def terminate_rendering(schedule, config, mqtts, reload=False):
   schedule.remove_all_jobs()
   sched_jobs = []
   mqtt_unsubscribe_and_stop(mqtts, config)
   #### Delete pending events ...

if __name__ == '__main__':
   setproctitle('Rendering')
   os.chdir(os.path.dirname(sys.argv[0]))
   # Initialize logging
   # stg.log_parameters['LEVEL'] = 'debug'  # Just for debugging
   setLogging()
   for signals in SIGNALS.values():
      for sig in signals:
         signal.signal(sig, signal_handler)
   # signal.signal(signal.SIGTERM, signal_handler)
   parser = argparse.ArgumentParser()
   parser.add_argument('-s', '--source', help='source MQTT broker', type=str)
   parser.add_argument('-t', '--topic',  help='topic on MQTT broker', type=str)
   args = parser.parse_args()
   
   # Initialize queues and lock
   cmd_queue = Queue()
   qpath = RENDER_QUEUE_PATH.format(os.getuid())
   _render_queue = PQueue(qpath, tempdir=qpath)
   lock_queue = Lock()
   logging.info("Current size of render queue: {}".format(_render_queue.qsize()))
   
   schedule = BackgroundScheduler()
   schedule.start()
   config, mqtts = initialize_rendering(schedule, args)
   
   while stg.run['PROCESS']:
      cmd = cmd_queue.get()
      if isinstance(cmd, str):
         cmd = cmd.upper()
      if cmd == 'PLAY':
         do_rendering()
      elif cmd == 'RELOAD':
         logging.info("Reload triggered: {}".format(cmd))
         terminate_rendering(schedule, config, mqtts, reload=True)
         stg = reload(stg)
         config, mqtts = initialize_rendering(schedule, args, reload=True)
      elif cmd == 'STOP':
         logging.info("Breaking main loop...")
         break
   terminate_rendering(schedule, config, mqtts)
      

