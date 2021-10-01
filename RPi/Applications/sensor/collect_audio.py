#!/usr/bin/python3

import os, sys, shutil, time
# import subprocess
from subprocess import Popen, run, CalledProcessError, DEVNULL
from multiprocessing import Process, Queue
from setproctitle import setproctitle
import _pickle
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

import pynormalize
import arff
import numpy as np
from scipy.io.wavfile import read
from featureextraction import extract_features

from collect import Collect

import logging
import warnings
warnings.filterwarnings("ignore", category=DeprecationWarning) 
warnings.filterwarnings("ignore", category=UserWarning) 

SMILE_CMD    = 'SMILExtract|-C|{}|-I|{}|-O|{}'

class ARFF_Features:
   def __init__(self, filename, source="other", length=0):
      self.source = source
      self.length = length
      try:
         with open(filename, "r") as arff_file:
            arff_data = arff.load(arff_file)                               
         for rec in zip(arff_data['attributes'], *arff_data['data']):
            if len(arff_data['data']) > 1:
               setattr(self, rec[0][0], list(rec[1:]))
            else:
               setattr(self, rec[0][0], rec[1])
      except:
         logging.warn("File {} was prematurely removed".format(filename))



class AudioDetectorHandler(FileSystemEventHandler):
   def __init__(self, queue):
      self.queue = queue
      
   def on_created(self, event): # When audio file of chunk is created
      timestamp_ms = int(time.time() * 1000)
      self.queue.put((timestamp_ms, event.src_path))


class CollectAudio(Collect):

   # Static variables for class
   # matrix_ip = '127.0.0.1' # Local device ip
   
   def __init__(self, config, queue, quantities=None):
      cfg = config['config']
      logging.debug("Initialize audio {}".format(cfg['desc']))
      super().__init__(config, queue, quantities)
      self.sequencer = None
      self.process  = None
      self.models   = None
      self.unit = cfg['sensor']
      self.desc = cfg['desc']
      self.smile_cfg = cfg['openSmile_cfg']
      
      # Load (unpickle) audio recognition models
      try:
         self.models = self.load_models(cfg['recog_cfg'])
      except:
         logging.warning("Error loading models @ {}".format(cfg['recog_cfg']))
      if not self.models:
         return
      # create and set process folders
      uid = os.getuid()
      self.path = dict()
      for dir in cfg['paths']: # Set and create folders if they don't exist
         self.path[dir] = cfg['base'].format(uid, dir)
         try:
            logging.info("Create folders {}".format(self.path[dir]))
            os.makedirs(self.path[dir], exist_ok=True)
         except:
            # self.stop()
            logging.info("Error creating folder {}".format(dir))
            return
      if not self.check_input_paths(cfg):
         return
      
      # setup observer & audio sequencer cmd
      self.observer = Observer()
      self.observer_queue = Queue()
      try:
         # self.process = Process(target=self.data_process, args=[queue])
         self.process = Process(target=self.data_process)
         self.process.name = 'Audio_Features'
      except:
         logging.warning("Failed to create process for audio feature extraction for {}".format(cfg['desc']))
         return
      detector = AudioDetectorHandler(self.observer_queue)
      self.observer.schedule(detector, path=self.path['source'])
      
      # setup audio sequencer, normalize, and openSmile commands and paths
      self.seq_cmd = cfg['seq_cmd'].split()
      self.seq_cmd.append(self.path[cfg['paths'][0]] + '/' + cfg['seq_file'])
      # Print debug information
      self.run = True
      logging.info("Connected to {}".format(self.desc))

   def check_input_paths(self, cfg):
      self.norm_path = None
      if len(cfg['paths']) < 2:  # At least input path and features path
         return False
      if len(cfg['paths']) == 2:  # No normalization
         cfg['recog_in']     = cfg['paths'][0]
         cfg['openSmile_in'] = cfg['paths'][0]
         return True
      inputs = {'recog_in': 'recog_norm', 'openSmile_in': 'smile_norm'}
      input_paths = cfg['paths'][0:-1]
      for opt, norm in inputs.items():
         if opt in cfg and cfg[opt] in input_paths:
            if cfg[opt] != input_paths[0]:
               # Set normalize path if one of inputs use normalize as input
               self.norm_path = self.path[cfg[opt]]
               setattr(self, norm, True) # record normalized input
            continue
         cfg[opt] = cfg['paths'][0]
      return True
         
      
   def load_models(self, path): # Load all recognition model
      name = lambda fname: fname.split("/")[-1].split(".gmm")[0]
      model_files = [os.path.join(path, fname) for fname in 
                       os.listdir(path) if fname.endswith('.gmm')]
      models = dict();
      for filename in model_files:
         try:
            with open(filename, 'rb') as fhandle:
               model = _pickle.load(fhandle)
               if model:
                  models[name(filename)] = model
         except:
            logging.info("Error loading model file {}".format(filename))
            pass
      # self.models = {name(f): self.read_model(f) for f in model_files}
      return models


   def process_features(self, filename):
      if not filename.endswith('.wav'):
         return 
      base = os.path.basename(filename)
      arff = os.path.splitext(base)[0]+'.arff'
      try:
         sound = 'other'
         normfile = None
         if self.norm_path: # Optional audio normalization
            pynormalize.process_files([filename], -20, self.norm_path)
            # os.remove(filename) # Cleanup
            normfile = os.path.join(self.norm_path, base)
         read_file = filename
         if hasattr(self, 'recog_norm'):
            read_file = normfile
         sr, audio = read(read_file)
         vector = extract_features(audio, sr)
         likelihood = { k: np.array(m.score(vector)).sum() for k, m in self.models.items() }
         logging.debug("likelihood {}".format(likelihood))
         sound, value = max(likelihood.items(), key=lambda key: key[1])
         if likelihood[sound] >= -1200:
            sound = 'other'
         featfile = os.path.join(self.path['features'], sound + '_' + arff)
         # featfile = os.path.join(self.path['feature'], arff)
         read_file = filename
         if hasattr(self, 'smile_norm'):
            read_file = normfile
         args = SMILE_CMD.format(self.smile_cfg, read_file, featfile).split('|')
         logging.debug("SMILE_CMD {}".format(args))
         smile = run(args, stdout=DEVNULL, stderr=DEVNULL)
         os.remove(filename) # Cleanup
         if normfile:
            os.remove(normfile) # Cleanup
         features = ARFF_Features(featfile, sound)
         os.remove(featfile) # Cleanup
         return features
      except:
         logging.warning("Exception in collectAudio.process_features")
      return None
  

   # def data_process(self, queue):
   def data_process(self):
      while self.run:
         try:
            # (timestamp, filename) = self.observer_queue.get()
            (timestamp, filename) = self.observer_queue.get()
            if filename is None:
               break
            features = self.process_features(filename)
            if features:
               super().send_data(timestamp, features)
         except KeyboardInterrupt:
            self.run = False


   def start(self, name=None):
      if name and 'PING' not in name:
         return
      if not self.process:
         # logging.info("Audio feature extraction process{} cannot start".format(config['config']['desc']))
         return
      self.process.start()   # Start audio features extraction process
      self.observer.start()
      try:
         #### DEBUG: afterwards remove 2 commnets
         logging.info("Audio sampling cmd {}".format(self.seq_cmd))
         self.sequencer = Popen(self.seq_cmd, stdout=DEVNULL, stderr=DEVNULL)
         # self.sequencer.name = 'Auditok'  # Set the name od the process
         # Print debug information
         logging.info("Starting {}".format(self.desc))
      except CalledProcessError as e:
         logging.warning("Failed to start sequencer for {}".format(self.desc))
         logging.warning("Error {}".format(e.output))
         # self.stop()


   def stop(self):
      logging.debug("Stopping child processes [CollectAudio]")
      if self.sequencer:
         if self.sequencer.is_alive():
            self.sequencer.terminate()
            self.sequencer.wait()
         logging.debug("Joined sequencer {}".format(self.sequencer))
         self.sequencer = None
      if self.observer:
         if self.observer.is_alive():
            self.observer.stop()
            self.observer.join(self.TIMEOUT)
         logging.debug("Joined observer {}".format(self.observer))
         self.observer = None
      if self.process:
         if self.process.is_alive():
            self.observer_queue.put((0, None))
            self.process.join(self.TIMEOUT)
         logging.debug("Joined process {}".format(self.process))
         self.process = None
      for folder in self.path.values():
         if folder:
            folder = os.path.dirname(folder)
            break
      if not folder:
         return
      try:
         shutil.rmtree(folder)
      except:
         logging.info("Error removing folder {}".format(folder))


   def join_all(self):
      pass
  
