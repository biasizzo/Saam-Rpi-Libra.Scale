#!/usr/bin/python3 -u

import os, sys, signal, time
import argparse
from ctypes import * # For supressing error messages
import pyaudio
import wave
from contextlib import contextmanager

SIGNALS = [signal.SIGTERM, signal.SIGINT, signal.SIGQUIT, signal.SIGABRT, signal.SIGHUP]
replacements = [("{N}", "{0}"), ("{start}", "{2}"), ("{end}", "{1}")]

def signal_handler(signal, frame):  # Signal handler to terminate
   global record
   record = False
   
# Error message supression ##########################################
ERROR_HANDLER_FUNC = CFUNCTYPE(None, c_char_p, c_int, c_char_p, c_int, c_char_p)
def py_error_handler(filename, line, function, err, fmt):
  pass

c_error_handler = ERROR_HANDLER_FUNC(py_error_handler)

@contextmanager
def noalsaerr():
  asound = cdll.LoadLibrary('libsound.so')
  asound.snd_lib_error_set_handler(c_error_handler)
  yield
  asound.snd_lib_error_set_handler(None)
#####################################################################

record = True  # Record audio signal

if __name__ == '__main__':
  # parser = argparse.ArgumentParser()
  parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('-d', '--duration', help='Audio sample duration in seconds', type=int, default=1, metavar="INT")
  parser.add_argument("-r", "--rate", help="Sampling rate of audio data", type=int, default=16000, metavar="INT")
  parser.add_argument("-c", "--channels", help="Number of channels of audio data", type=int, default=1, metavar="INT")
  parser.add_argument("-w", "--width", help="Number of bytes per audio sample", type=int, default=2, metavar="INT")
  parser.add_argument('-o', '--output', required=True, help='Output file template')
  
  # group = OptionGroup(parser, "[Audio parameters]", "Define audio parameters")
  # group.add_option("-r", "--rate", dest="sampling_rate", help="Sampling rate of audio data [default: %default]", type=int, default=16000, metavar="INT")
  # group.add_option("-c", "--channels", dest="channels", help="Number of channels of audio data [default: %default]", type=int, default=1, metavar="INT")
  # group.add_option("-w", "--width", dest="sample_width", help="Number of bytes per audio sample [default: %default]", type=int, default=2, metavar="INT")
  # parser.add_option_group(group)
  # (opts, args) = parser.parse_args(argv)
  
  args = parser.parse_args()

  print(args)

  for sig in SIGNALS:
     signal.signal(sig, signal_handler)  
  # signal.signal(signal.SIGHUP, signal_handler)

  chunk = args.rate >> 2
  old = sys.stderr
  sys.stderr = object
  # Error message supression ##########################################
  # print("Start supressing")
  # with noalsaerr():
  audio = pyaudio.PyAudio()
  # print("After supressing")
  #####################################################################
  audio_format = audio.get_format_from_width(args.width)
  stream = audio.open( format = audio_format,
                       channels = args.channels,
                       rate     = args.rate,
                       input    = True,
                       output   = False,
                       frames_per_buffer = chunk )
  sys.stderr = old
  # count = 5
  index = 0 
  for replace in replacements:
     args.output = args.output.replace(*replace)
  print("rate={}, duration={}, chunk={}".format(args.rate, args.duration, chunk))
  while record:
     try:
        frames = []
        for i in range(int(args.rate*args.duration/chunk)):
           frms = stream.read(chunk)
           frames.append(frms)
        ts = round(time.time(), 3)
        index &= 0xFFF
        index += 1
        filename = args.output.format(index, ts, ts-args.duration)
        wf = wave.open(filename, "wb")
        wf.setnchannels(args.channels)
        wf.setsampwidth(args.width)
        wf.setframerate(args.rate)
        wf.writeframes(b''.join(frames))
        wf.close()
        # count -= 1
        # if count < 1:
        #    record = False
     except Exception as e:
        print("Got exception: {}".format(e))
        record = False
  stream.stop_stream()
  stream.close()
  audio.terminate()
  print("All done")
