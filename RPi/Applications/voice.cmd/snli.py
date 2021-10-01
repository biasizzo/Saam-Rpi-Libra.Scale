# -*- coding: UTF-8 -*-

# from __future__ import print_function
import sys
import signal
# import datetime
import argparse

import snowboydecoder_arecord as snowboydecoder

from saam_constants import BACKGROUND_MODE, ACTIVE_MODE, ALERT_MODE, CALL_MODE, CALLBACK, DEFAULT_SENSITIVITY, DEFAULT_REPEAT_TIMEOUT, DEFAULT_REPEAT_NUMBER
from saam_config import get_models_callbacks, model_files_ok, SUPPORTED_LANGS
from saam_callbacks import SAAM

SLEEP_TIME = 0.03

def restricted_float(x, m=0, M=1):
    x = float(x)
    if x < m or x > M:
        raise argparse.ArgumentTypeError("{} not in range [{}, {}]".format(x, m, M))
    else:
        return x


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', '--language', required=True, choices=SUPPORTED_LANGS, help='input/output language for SAAM agent')
    parser.add_argument('-s', '--sensitivity', required=False, type=restricted_float, default=DEFAULT_SENSITIVITY, help='speech recognition sensitivity')
    parser.add_argument('-t', '--timeout', required=False, type=int, choices=range(2,10), default=DEFAULT_REPEAT_TIMEOUT, help='keyword repeat timeout')
    parser.add_argument('-r', '--repeat', required=False, type=int, choices=range(1,5), default=DEFAULT_REPEAT_NUMBER, help='keyword repeat count')
    parser.add_argument('-v', '--verbose', action='store_true', help='enable verbose feedback')
    
    args = parser.parse_args()
    language = args.language
    sensitivity = args.sensitivity
    
    if not model_files_ok(language):
        exit(1)

    SAAM.language = language
    SAAM.espeak.voice = language
    SAAM.verbose = args.verbose
    SAAM.repeat_timeout = args.timeout
    SAAM.repeat_count = args.repeat

    # capture SIGINT signal, e.g., Ctrl+C
    signal.signal(signal.SIGINT, SAAM.exit_signal_handler)
    SAAM.saam_mode = BACKGROUND_MODE
    
    while True:
        SAAM.lock.acquire()
        if SAAM.saam_mode == BACKGROUND_MODE:
            print('SAAM is running in mode "{}"'.format(SAAM.saam_mode))
            models, callbacks = get_models_callbacks(language, SAAM.saam_mode)
            detector = snowboydecoder.HotwordDetector(models, sensitivity=[sensitivity]*len(models))
            SAAM.interrupted = False
            SAAM.lock.release()
            detector.start(detected_callback=callbacks,
                           interrupt_check=SAAM.interrupt_callback,
                           sleep_time=SLEEP_TIME)
            detector.terminate()
        elif SAAM.saam_mode == ACTIVE_MODE:
            print('SAAM is running in mode "{}"'.format(SAAM.saam_mode))
            models, callbacks = get_models_callbacks(language, SAAM.saam_mode)
            detector = snowboydecoder.HotwordDetector(models, sensitivity=[sensitivity]*len(models))
            SAAM.interrupted = False
            SAAM.lock.release()
            detector.start(detected_callback=callbacks,
                           interrupt_check=SAAM.interrupt_callback,
                           sleep_time=SLEEP_TIME)
            detector.terminate()
        elif SAAM.saam_mode == ALERT_MODE:
            print('SAAM is running in mode "{}"'.format(SAAM.saam_mode))
            models, callbacks = get_models_callbacks(language, SAAM.saam_mode)
            detector = snowboydecoder.HotwordDetector(models, sensitivity=[sensitivity]*len(models))
            SAAM.interrupted = False
            SAAM.lock.release()
            detector.start(detected_callback=callbacks,
                           interrupt_check=SAAM.interrupt_callback,
                           sleep_time=SLEEP_TIME)
            detector.terminate()
        elif SAAM.saam_mode == CALL_MODE:
            print('SAAM is running in mode "{}"'.format(SAAM.saam_mode))
            models, callbacks = get_models_callbacks(language, SAAM.saam_mode)
            detector = snowboydecoder.HotwordDetector(models, sensitivity=[sensitivity]*len(models))
            SAAM.interrupted = False
            SAAM.lock.release()
            detector.start(detected_callback=callbacks,
                           interrupt_check=SAAM.interrupt_callback,
                           sleep_time=SLEEP_TIME)
            detector.terminate()
        else:
            SAAM.lock.release()
            break
    print('SAAM event loop is finished')
