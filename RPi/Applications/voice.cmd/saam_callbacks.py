# -*- coding: UTF-8 -*-

import sys
import threading
from threading import Event, Thread, Timer, Lock
import time

### Added: 2019.09.13:  importing from subprocess for process execution
from subprocess import Popen, PIPE

from espeakng import ESpeakNG
import sounddevice as sd
import soundfile as sf

import mqtt
from saam_constants import BACKGROUND_MODE, ACTIVE_MODE, ALERT_MODE, CALL_MODE, DEFAULT_REPEAT_TIMEOUT, DEFAULT_REPEAT_NUMBER, OK_SAAM, HELP, STOP, HELP_REPEAT, CALL, ERROR, CALL_FAMILY, CALL_NEIGHBOUR, REPEAT_COMMAND
from saam_responses import get_response

ping = sf.read('resources/sounds/ping.wav')
clickoff = sf.read('resources/sounds/clickoff.wav')

### Added: 2019.09.13 - espeak-ng library has problems with output
espeak_cmd = "espeak-ng -s {} -v {} --stdout"

class SAAM:
    interrupted = False
    saam_mode = None
    language = None
    repeat_timeout = DEFAULT_REPEAT_TIMEOUT
    repeat_count = DEFAULT_REPEAT_NUMBER 
    verbose = False
    # count = 0
    stopper = None
    counter = {}

    # Added: 2019.11.14 - moved from global declaration
    def __init__(self):
      espeak = ESpeakNG()
      espeak.speed = 110
      mqtt_client = mqtt.MQTT(debug=False)
      lock = Lock()


    def increase_count(command):
        if command in SAAM.counter:
            SAAM.counter[command] += 1
        else:
            SAAM.counter[command] = 1
        
    def reset_command_counter():
        SAAM.counter = {}

    def return_to_background():
        SAAM.lock.acquire()
        if SAAM.verbose:
            print('-----> interrupt after {} seconds '.format(SAAM.stopper.interval))
        SAAM.stopper.cancel()
        SAAM.interrupted = True
        SAAM.reset_command_counter()
        sd.play(clickoff[0], clickoff[1], blocking=True)
        SAAM.saam_mode = BACKGROUND_MODE
        SAAM.lock.release()

    def format_command_mqtt(command):
        return 'command_{}'.format(command.replace(' ', '_').lower())
    
    def goto_mode(mode, command, timeout=True):
        SAAM.lock.acquire()
        if SAAM.verbose:
            print('----->  countdown cancelled!')
            print("Transition mode: {} ===> {}".format(SAAM.saam_mode, mode))
        SAAM.stopper.cancel()
        SAAM.interrupted = True
        SAAM.reset_command_counter()
        SAAM.saam_mode = mode
        SAAM.say(command, SAAM.language)
        # Added: 2019.09.14
        SAAM.mqtt_client.send_to_topic(SAAM.format_command_mqtt(command))
        SAAM.mqtt_client.send_to_topic(SAAM.format_command_mqtt(command), 'saam/voice_command')
        if timeout:
            if SAAM.verbose:
                print('----->  countdown started!')
            SAAM.stopper = Timer(SAAM.repeat_timeout, SAAM.return_to_background)
            SAAM.stopper.start()
        SAAM.lock.release()
        
    def wait_for_repeat():
        SAAM.lock.acquire()
        if SAAM.stopper:
            SAAM.stopper.cancel()
        sd.play(ping[0], ping[1], blocking=True)
        if SAAM.verbose:
            SAAM.say(REPEAT_COMMAND, SAAM.language)
            
        SAAM.stopper = Timer(SAAM.repeat_timeout, SAAM.return_to_background)
        SAAM.stopper.start()
        SAAM.lock.release()
    
    def say(command, language):
        ### Added 2020.01.13: use prerecorded messages
        try:
            sound = snd_path.format(lang, snd)
            wave.open(sound, 'rb')
            wav.close()
            play  = Popen(['aplay','-q', sound])
        except:
            ### Added: 2019.09.13: calling external program for speach generation
            espeak_args = espeak_cmd.format(SAAM.espeak.speed, SAAM.espeak.voice).split()
            espeak_args.append(get_response(command, SAAM.language))
            speak = Popen(espeak_args, stdout=PIPE, stderr=PIPE)
            play  = Popen(['aplay','-q'], stdin=speak.stdout)
            speak.stdout.close()
        play.wait()
        
    def interrupt_callback():
        return SAAM.interrupted

    def exit_signal_handler(signal, frame):
        # capture SIGINT signal, e.g., Ctrl+C
        SAAM.lock.acquire()
        SAAM.interrupted = True
        SAAM.saam_mode = None
        SAAM.lock.release()
        
    def stop_system():
        sys.exit(0)

    def ok_saam():
        print("Current mode: {} : ok_saam".format(SAAM.saam_mode))
        SAAM.increase_count(OK_SAAM)
        if SAAM.counter[OK_SAAM] < SAAM.repeat_count:
            SAAM.wait_for_repeat()
        else:
            SAAM.goto_mode(ACTIVE_MODE, OK_SAAM)
            
    def saam_stop():
        print("Current mode: {} : saam_stop".format(SAAM.saam_mode))
        SAAM.increase_count(STOP)
        if SAAM.counter[STOP] < SAAM.repeat_count:
            SAAM.wait_for_repeat()
        else:
            SAAM.goto_mode(BACKGROUND_MODE, STOP, timeout=False)
        
    def help():
        print("Current mode: {} : help".format(SAAM.saam_mode))
        SAAM.increase_count(HELP)
        if SAAM.counter[HELP] < SAAM.repeat_count:
            SAAM.wait_for_repeat()
        else:
            SAAM.goto_mode(ALERT_MODE, HELP, timeout=False)
     
    def call():
        print("Current mode: {} : call".format(SAAM.saam_mode))
        SAAM.increase_count(CALL)
        if SAAM.counter[CALL] < SAAM.repeat_count:
            SAAM.wait_for_repeat()
        else:
            SAAM.goto_mode(CALL_MODE, CALL)
    
    def call_family():
        print("Current mode: {} : family".format(SAAM.saam_mode))
        SAAM.increase_count(CALL_FAMILY)
        if SAAM.counter[CALL_FAMILY] < SAAM.repeat_count:
            SAAM.wait_for_repeat()
        else:
            SAAM.goto_mode(BACKGROUND_MODE, CALL_FAMILY, timeout=False)
    
    def call_neighbour():
        print("Current mode: {} : neighbor".format(SAAM.saam_mode))
        SAAM.increase_count(CALL_NEIGHBOUR)
        if SAAM.counter[CALL_NEIGHBOUR] < SAAM.repeat_count:
            SAAM.wait_for_repeat()
        else:
            SAAM.goto_mode(BACKGROUND_MODE, CALL_NEIGHBOUR, timeout=False)
        
    def error():
        print("Current mode: {} : error".format(SAAM.saam_mode))
        SAAM.increase_count(ERROR)
        if SAAM.counter[ERROR] < SAAM.repeat_count:
            SAAM.wait_for_repeat()
        else:
            SAAM.goto_mode(BACKGROUND_MODE, ERROR, timeout=False)
        
