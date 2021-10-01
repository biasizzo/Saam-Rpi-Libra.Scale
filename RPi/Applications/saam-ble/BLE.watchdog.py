#!/usr/bin/python3

from threading import Timer

import sys, os, time, ssl, signal
import logging, logging.handlers
from multiprocessing import Queue, Lock
import paho.mqtt.client as mqtt
from setproctitle import setproctitle
from importlib import reload  

import settings as stg


SIGNALS = dict(
  TERMINATE = [signal.SIGTERM, signal.SIGINT, signal.SIGQUIT, signal.SIGABRT]
)
#              interval, count
INTERVALS = [ (300,        3),   # 5 min,   3x
              (900,        3),   # 15 min,  3x
              (3600,       2),   # 1 hour,  2x
              (21600,      4) ]  # 6 hour,  indefinetly
#               (43200,      4),   # 1/2 day, 4x
#               (259200,     0) ]  # 3 days,  indefinitely

quantities = {'sens_bed':          ['_amb', '_egw', '_app']} # _accel is implied
#            {'sens_amb_1_temp':   None,
#             'sens_amb_1_press:   None,
#             'sens_belt_accel:    ['_amb', '_egw', '_app']}

cmd_queue  = None
CMD = {
    'RPi3B+': {'rev': '1.3', 'cmd': 'uhubctl -l 1-1 -p 2 -a 2'},
    'RPi4B' : {'rev': None,  'cmd': 'uhubctl -l 1-1 -a 2'}
}
# CMD        = 'sudo USB_power-cycle.sh'


class Watchdog(object):
    # Watchdog initialization
    def __init__(self, intervals, function, *args, **kwargs):
        self._timer    = None
        self.intervals = intervals
        self.function  = function
        self.args      = args
        self.kwargs    = kwargs
        self.running   = False # Could use self._timer instead (False ==> None)
        self.idx       = 0
        self.cnt       = -1
        self.max       = -1
        if not self.intervals:
            return
        if not isinstance(self.intervals, list):
            return
        self.max       = len(self.intervals) - 1

    # Method triggered by watchdog timer
    def _run(self):
        self.running = False
        if self.max < 0:  # Should not happen
            return
        self.start()
        self.function(*self.args, **self.kwargs)

    # Adjust interval structure and start timer
    def start(self):
        if self.max < 0:  # Don't start if invalid interval structure
            return
        if self.running:
            return
        # logging.debug("Watchdog start idx={}, cnt={}".format(self.idx, self.cnt))
        # logging.debug("Watchdog start interval: {}".format(self.intervals[self.idx]))
        interval, count = self.intervals[self.idx]
        if count:
            self.cnt += 1
            if self.cnt >= count and self.idx < self.max:
                self.cnt = 0
                self.idx += 1
                interval, count = self.intervals[self.idx]
        # logging.debug("Watchdog start updated idx={}, cnt={}".format(self.idx, self.cnt))
        # logging.debug("Watchdog start updated interval: {}".format(self.intervals[self.idx]))
        self._timer = Timer(interval, self._run)
        self._timer.start()
        self.running = True

    def restart(self):
        if not self.running:
            return
        if self._timer:
            self._timer.cancel()
            self._timer = None
        self.running = False
        if self.max < 0:  # Should not happen
            return
        self.idx       = 0
        self.cnt       = -1
        self.start()
        
    def stop(self):
        if self._timer:
            self._timer.cancel()
            self._timer = None
        self.running = False


def signal_handler(signalNumber, frame):  # Signal handler to reload configuration
    logging.debug("Got signal {}".format(signalNumber))
    if signalNumber in SIGNALS['TERMINATE']: # Terminate 
        logging.info("Terminate")
        stg.run['PROCESS'] = False;
        cmd_queue.put('STOP')
        return
    # if signalNumber in SIGNALS['RELOAD']:    # Reload configuration
    #    logging.info("Reload configuration")
    #    try:                              # Actually just location_id
    #       locfile = open('/boot/SAAM/location.id', 'r')
    #       line = locfile.readline().strip()
    #       locfile.close()
    #       if line:
    #          stg.run['LOCATION_ID'] = line
    #    except:
    #       pass
    #    # cmd_queue.put('RELOAD')
    #    return
   

def setLogging(procName):
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
            file = file.format(procName)
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
   # global watchdog
   try:
      watchdog.restart()
      logging.debug("Got MQTT message: Watchdog timer restart")
   except:
      logging.warning("Error restarting watchdog timer")


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

def mqtt_initialize(mqtt_set):
    logging.info("Initialize {}".format(mqtt_set))
    MQTT_ID = "{}_{}".format(stg.run['LOCATION_ID'], os.getpid())
    mqtts = dict()
    for mqtt_name in mqtt_set:
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

def mqtt_disconnect(mqtts):
    logging.info("Disconnect {}".format(mqtts))
    for client in mqtts.values():
        client.disconnect()
    

def mqtt_subscribe_and_start(mqtts, quantities):
    logging.debug("MQTT channels: {}".format(mqtts))
    if not mqtts or not quantities:
        return
    for quantity, suff in quantities.items():
        logging.debug("Quantity: {}, channels: {}".format(quantity, stg.report[quantity][3]))
        if not suff:
            suff = ['']
        quant, _, _, channels = stg.report[quantity]
        if not isinstance(channels, list):
            channels = [channels]
        for ch in channels:
            logging.debug("Channels: {}".format(ch))
            if ch not in mqtts:
                continue
            chnl = stg.transmit[ch]
            base = "{}/{}/{}".format(chnl['TOPIC'], stg.run['LOCATION_ID'], quant)
            topics = [ "{}{}/#".format(base,s) for s in suff ]
            for topic in topics:
                logging.info("Subscribe on {} to topic: {}".format(chnl['HOST'], topic))
                mqtts[ch].subscribe(topic, 1)
    logging.info("Loop start for: {}".format(mqtts))
    for client in mqtts.values():
        client.loop_start()

def mqtt_unsubscribe_and_stop(mqtts, quantities):
    if not mqtts or not quantities:
       return
    for quantity, suff in quantities.items():
        logging.debug("Quantity: {}, channels: {}".format(quantity, stg.report[quantity][3]))
        if not suff:
            suff = ['']
        quant, _, _, channels = stg.report[quantity]
        if not isinstance(channels, list):
            channels = [channels]
        for ch in channels:
            logging.debug("Channels: {}".format(ch))
            if ch not in mqtts:
                continue
            chnl = stg.transmit[ch]
            base = "{}/{}/{}".format(chnl['TOPIC'], stg.run['LOCATION_ID'], quant)
            topics = [ "{}{}/#".format(base,s) for s in suff ]
            for topic in topics:
                logging.info("Subscribe on {} to topic: {}".format(chnl['HOST'], topic))
                mqtts[ch].unsubscribe(topic)
    logging.info("Loop stop for: {}".format(mqtts))
    for client in mqtts.values():
        client.loop_stop()


def trigger(cmd):
    logging.warning("Issued command: {}".format(cmd))
    ret = os.system( "sudo {} > /dev/null".format(cmd))
    if ret:
        logging.error("Command failed!")


if __name__ == '__main__':
    setproctitle('BLEWatchdog')
    os.chdir(os.path.dirname(sys.argv[0]))
    # Initialize logging
    # stg.log_parameters['LEVEL'] = 'debug'  # Just for debugging
    setLogging('BLEWatchdog')
    ######## DEBUG ####################################
    # stg.run['LOCATION_ID'] = 'BG50'
    # modify = list(stg.report['sens_bed'])
    # modify[3] = 'mqtt_dev'
    # stg.report['sens_bed'] = tuple(modify)
    ##################################################
    with open('/proc/device-tree/model', 'r') as fh:
        model=fh.readline().strip('\0')
    model = model.replace('aspberry Pi ','Pi').replace(' Model ', '')
    model = model.replace(' Plus', '+').replace('Rev ', '').split()
    cmd = "echo"
    if model[0] in CMD:
        if not CMD[model[0]]['rev'] or model[1] >= CMD[model[0]]['rev']:
            cmd = CMD[model[0]]['cmd']
    cmd_queue = Queue()
    for signals in SIGNALS.values():
        for sig in signals:
            signal.signal(sig, signal_handler)
 
    # quantities = [q for q in quantities if q in stg.report]
    quantities = {q: val for q, val in quantities.items() if q in stg.report}
    get_list = lambda x: x if isinstance(x, list) else [x]
    ch_quant = {ch for q in quantities for ch in get_list(stg.report[q][3])}
    channels = ch_quant.intersection(set(stg.transmit.keys()))
    logging.debug("Channels: {}".format(channels))
    watchdog = Watchdog(INTERVALS, trigger, cmd)
    mqtt = mqtt_initialize(channels)
    mqtt_subscribe_and_start(mqtt, quantities)
    watchdog.start()
    while stg.run['PROCESS']:
        cmd = cmd_queue.get()
        if isinstance(cmd, str):
            cmd = cmd.upper()
        elif cmd == 'STOP':
            logging.info("Breaking main loop...")
            break
    watchdog.stop()
    mqtt_unsubscribe_and_stop(mqtt, quantities)
    mqtt_disconnect(mqtt)
    
