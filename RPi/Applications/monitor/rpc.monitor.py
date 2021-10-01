#!/usr/bin/python3

import zerorpc
import io
import psutil
import logging, logging.handlers
import signal
import socket
import json

from subprocess import Popen, PIPE
from settings import run, sensors, transmit, report, channels, log_parameters

HOST="0.0.0.0"
LOCALHOST="127.0.0.1"
PORT=4242
USER="saam"
SUDO="/usr/bin/sudo -u {}"
PROCTBL = {'data': 3, 'error': 2, 'keep-alive': 1}
PROGRAMS=['Ambient']
SERVICES=['autossh.service']
HOTWORDS="/home/saam/SAAM/voice.cmd/train.hotwords.sh"
LOCATION="sudo /usr/local/sbin/set.location {}"
RESTART ="sudo systemctl restart {}"

log_parameters = dict(
   FILE="/home/saam/SAAM/monitor/output.log",
   # FILE="",
   FORMAT = "%(asctime)s %(levelname)s: %(message)s",
   DATE = "%b %d %H:%M:%S",
   LEVEL = "debug"
)

def setLogging():
    root = logging.getLogger()
    level = getattr(logging, log_parameters['LEVEL'].upper(), None)
    format = None
    if 'FORMAT' in log_parameters.keys():
        datefmt = None
        if 'DATE' in log_parameters.keys():
            datefmt = log_parameters['DATE']
        format = logging.Formatter(fmt=log_parameters['FORMAT'], datefmt=datefmt)
    if 'FILE' in log_parameters.keys():
        file = log_parameters['FILE']
        # Remove old logging handlers (to stderr)
        for hnd in root.handlers:
            root.removeHandler(hnd)
        if file:
            # logging to file
            file = file.format('rpc.monitor')
            size = 0
            if 'SIZE' in log_parameters:
                size = log_parameters['SIZE']
            size = size * 1024 * 1024 # Megabytes
            cnt  = 0
            if 'CNT' in log_parameters:
                cnt = log_parameters['CNT']
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

def checkArgument(arg):
   if "matrix" in arg:
      return True
   return False

class AmbientSensor(object):
   def __init__(self):
      self.sockets = self.getOpenSockets()
      self.lookup = {quantity: name for name, cfg in sensors.items()
                        if isinstance(cfg, dict) and 'quantity' in cfg
                           for quantity in cfg['quantity']}
      
   def setLocationID(self, name=None):
      """Set location ID if changed"""
      if name and run['LOCATION_ID'] != name:
        run['LOCATION_ID'] = name
        logging.debug("Change location ID")
        try:
           set_location = Popen(LOCATION.format(name).split())
           set_location.wait(5)
        except:
           return "Failed to set new location"
        # Send HUP signal to programs (Ambient DAQ, reverse ssh, ...)
        logging.debug("Sending SIGHUP signal")
        for proc in psutil.process_iter():
           for prog in PROGRAMS:
              if prog in proc.name():
                 proc.send_signal(signal.SIGHUP)
        # Run programs due to location change
        logging.debug("Restart services")
        for service in SERVICES:
           try:
              restart=Popen(RESTART.format(service).split())
              restart.wait(10)
           except:
              logging.info("Failed to restart service {}".format(service))
              # pass

   def getOpenSockets(self):
      # Get list of port, ip, and pid of remote connection addresses
      # sockets = {(c.raddr.port, c.raddr.ip): c.pid for c in psutil.net_connections() if c.pid is not None and c.raddr}
      net = [c for c in psutil.net_connections() if c.pid is not None and c.raddr]
      # sockets = {c.raddr.port:(c.raddr.ip, c.pid) for c in psutil.net_connections() if c.pid is not None and c.raddr}
      sockets={}
      for n in net:
         soc = (n.raddr.ip, n.raddr.port, psutil.Process(n.pid).name())
         if soc in sockets:
            sockets[soc] += 1
         else:
            sockets[soc] = 1
      return sockets
       
   def getFPGAInfo(self):
      info = dict()
      FPGA_INFO = "/usr/share/matrixlabs/matrixio-devices/fpga_info"
      proc = Popen([FPGA_INFO], stdout=PIPE, stderr=PIPE)
      for line in proc.stdout.readlines():
         opts = line.strip().decode('utf-8').split('=')
         if len(opts) < 2:
            continue
         name = opts[0].strip().split()
         if (len(name) < 2) or (name[0].upper() != "FPGA"):
            continue
         info["_".join(name[1:])] = "=".join(opts[1:]).strip()
      return info

   def checkMatrixIO(self):
      """Check if MatrixIO board is working"""
      info = self.getFPGAInfo()
      id  = 'IDENTIFY'
      ver = 'VERSION'
      if id not in info or info[id] != '5c344e8':
         return "MatrixCreator board not detected. Verify board placement"
      if ver not in info or info[ver] < '10008':
         return "MetrixCreator board version too low"

   def checkSensors(self):
      """Check sensor processes"""  
      # Filter used MALOS sensors
      sens_set = {self.lookup[sens] for sens in report if sens in self.lookup}
      for sens in sens_set:
         if sensors[sens]['type'] != 'MALOS': # skip non MALOS sensor items
            continue
         cfg = sensors[sens]['config']
         error = 0
         for label, offset in PROCTBL.items():
            try:
               port = cfg['base']+offset
               name = cfg['driver'].capitalize() + '_' + label
               soc = (LOCALHOST, port, name)
               logging.debug("socket: {}".format(soc))
               self.sockets[soc] -= 1
               if self.sockets[soc] >= 0:
                  continue
            except:
               pass
            # err[s['config']['driver']] |= (1 << offset)
            error |= (1 << offset)
         if error:
            return "{} error: {}".format(cfg['desc'], error)

   def checkTransmission(self):
      """Check data transmission processes"""  
      # Filter used MQTT channels
      get_list = lambda x: x if isinstance(x, list) else [x]
      ch_set = {ch for name, dest in report.items() if name in self.lookup
                     for ch in get_list(dest[3]) if ch in transmit}
            
      # err = { s['config']['driver']: 0 for key, s in sensors.items() if s and s['type'] == 'MALOS' }
      """Check mqtt transmit connections"""
      for ch in ch_set:
         t = transmit[ch]
         if t['TYPE'] != 'MQTT': # skip non MQTT transmissions
            continue
         soc = (socket.gethostbyname(t['HOST']), t['PORT'], 'Ambient')
         try:
            logging.debug("socket: {}".format(soc))
            self.sockets[soc] -= 1
            if self.sockets[soc] >= 0:
               continue
         except:
            pass
         msg = "{} error[{}]: {} @ {}:{}"
         return msg.format(t['TYPE'], 'Ambient', ch, t['HOST'], t['PORT'])
         

   def checkBLE(self):
      """Check BLE data transmisson connections"""  
      JSONFILE="/home/saam/SAAM/saam-ble/config.json"
      try:
         with open(JSONFILE) as jsonfile:
            cfg = json.load(jsonfile)
      except:
         return "BLE module error: cannot read configuration file"
      # Filter used MQTT channels
      get_list = lambda x: x if isinstance(x, list) else [x]
      ch_set = {ch for dev in cfg['devices'] if 'channels' in dev
                   for ch in get_list(dev['channels']) if ch in transmit}
      for ch in ch_set:
         t = transmit[ch]
         if t['TYPE'] != 'MQTT': # skip non MQTT transmissions
            continue
         soc = (socket.gethostbyname(t['HOST']), t['PORT'], 'runSaam.py')
         try:
            logging.debug("socket: {}".format(soc))
            self.sockets[soc] -= 1
            if self.sockets[soc] >= 0:
               continue
         except:
            pass
         msg = "{} error[{}]: {} @ {}:{}"
         return msg.format(t['TYPE'], 'BLE', ch, t['HOST'], t['PORT'])
            
   def checkRendering(self):
      """Check rendering MQTT connections"""  
      # Filter used MQTT channels
      get_list = lambda x: x if isinstance(x, list) else [x]
      try:
         chnls = channels['render'][1]
         ch_set = {ch for ch in get_list(chnls) if ch in transmit}
      except:
         return
      for ch in ch_set:
         t = transmit[ch]
         if t['TYPE'] != 'MQTT': # skip non MQTT transmissions
            continue
         soc = (socket.gethostbyname(t['HOST']), t['PORT'], 'Rendering')
         try:
            logging.debug("socket: {}".format(soc))
            self.sockets[soc] -= 1
            if self.sockets[soc] >= 0:
               continue
         except:
            pass
         msg = "{} error[{}]: {} @ {}:{}"
         return msg.format(t['TYPE'], 'Rendering', ch, t['HOST'], t['PORT'])
            
      
class AmbientSensor_RPC(object):
   def test(self, name=None):
      sensor = AmbientSensor()
      """Ambient sensor BIST"""
      logging.info("Ambient sensor test on location {}".format(name))
      # setLogging()
      """First set location ID if changed"""
      msg = sensor.setLocationID(name)
      if msg:
         return msg
      """Check MatrixIO board operation"""
      msg = sensor.checkMatrixIO()
      if msg:
         return msg
      """Check ambient sensor data acquisition"""  
      msg = sensor.checkSensors()
      if msg:
         return msg
      """Check ambient sensor MQTT connections"""  
      msg = sensor.checkTransmission()
      if msg:
         return msg
      """Check BLE MQTT connections"""  
      msg = sensor.checkBLE()
      if msg:
         return msg
      """Check rendering MQTT connections"""  
      msg = sensor.checkRendering()
      if msg:
         return msg
      return "OK"

   def train_voiceCMD(self, name=None):
      """Train Voice Command Module"""
      logging.info("Train voice command")
      try:
         cmd = SUDO.format(USER).split() + [HOTWORDS]
         train = Popen(cmd)
      except:
         return "Failed to start training session"
      return "OK"

   def mood(self, name=None):
      """Mood Box settings"""
      logging.info("Ambient Sensor mood")
      return "Mood triggered"

if __name__ == '__main__':
   setLogging()
   RPC = zerorpc.Server(AmbientSensor_RPC())
   RPC.bind("tcp://{}:{}".format(HOST, PORT))
   RPC.run()
