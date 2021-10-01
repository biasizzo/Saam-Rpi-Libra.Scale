#!/usr/bin/python3

import sys
import os
import io
import time
import wave
import pathlib
import argparse
import base64
import requests
import subprocess
import threading

from itertools import zip_longest

from saam_config import SETTINGS

MAX_LEARN = 2
endpoint = "https://snowboy.kitt.ai/api/v1/train/"
send = True

# Sound dirname
snd_path="resources/sounds/train/{}/{}.wav"
# Basename of wav files for training
instruct_wave=["train", "", "built", "repeat"]

# Instruction text in different languages
info = dict()
info['en'] = dict(
  instruct = [ "Train voice recognition models",
               "",
               "The model is built",
               "Word is not recognized. Repeat training"],
  train    = [ "Say the word {}",
               "Repeat the word {}",
               "Again Repeat the word {}",
               "Say the word {} for verification"],
  keywords = { 'saam'     : "Saam",
               'help'     : "Help",
               'call'     : "Call",
               'family'   : "Family",
               'neighbour': "Neighbour",
               'saam stop': "Saam Stop"   }
)

info['sl'] = dict(
  instruct = [ "Učenje prepoznave besed",
               "",
               "Model je zgrajen",
               "Beseda ni bila prepoznana. Ponovi Učenje"],
  train    = [ "Izgovori besedo {}",
                "Ponovi besedo {}",
                "Še enkrat ponovi besedo {}",
                "Za preverjanje ponovi besedo {}"],
  keywords = { 'saam'     : "Saam",
               'help'     : "Na pomoč",
               'call'     : "Kliči",
               'family'   : "Družino",
               'neighbour': "Sosede",
               'saam stop': "Saam Stop"   }
)

info['bg'] = dict(
  instruct = [ "Обучете модели за разпознаване на глас",
               "",
               "Моделът е изграден",
               "Думата не се разпознава. Повторете тренировките"],
  train    = [ "Кажете думата {}",
               "Повторете думата {}",
               "Отново повторете думата {}",
               "Кажете думата {} за проверка"],
  keywords = { 'saam'     : "Saam",
               'help'     : "Помогне",
               'call'     : "Обади се",
               'family'   : "Cемейство",
               'neighbour': "Съсед",
               'saam stop': "Saam Stop"   }
)

info['de'] = dict(
  instruct = [ "Modelle zur Spracherkennung trainieren",
               "",
               "Das Modell ist gebaut",
               "Word wird nicht erkannt. Wiederholen Sie das Training"],
  train    = [ "Sagen Sie das Wort {}",
               "Wiederhole das Wort {}",
               "Wiederholen Sie das Wort {}",
               "Wiederholen Sie das Wort {} zur Bestätigung"],
  keywords = { 'saam'     : "Saam",
               'help'     : "Hilfe",
               'call'     : "Anruf",
               'family'   : "Familie",
               'neighbour': "Nachbar",
               'saam stop': "Saam Stop"   }
)


# Basename of pre and post wav files for hotword training
train_word_wave=[
   ["say1", "", ""],
   ["say2", "", ""],
   ["say3", "", ""],
   ["vfy_pre", "", "vfy_post"] ]

espeak_cmd = "espeak-ng -s 110 -v {} --stdout"

def get_training_languages():
  langs = []
  for lang in info:
    langs.append(lang)
  return langs

TOKEN = "dead0011223344556677889900aabbccddeedead"  # for some snowboy.kitt.ai account

LANGUAGES = get_training_languages()

INIT=0
LEARN=1
NEXT=2
BAD=3
DONE=4

def is_file(path):
  file = pathlib.Path(path)
  return file.exists()

def play_sounds(sounds, lang):
  if isinstance(sounds, str): 
    sounds = [sounds]
  if not isinstance(sounds, list):
    return None
  waves = []
  buff = None
  output = None
  try:
    for snd in sounds:
      if snd:
        waves.append(wave.open(snd_path.format(lang, snd), 'rb'))
    if len(waves) == 0:  # Empty sound list
      return None
    buff = io.BytesIO()
    output = wave.open(buff, 'wb')
    output.setparams(waves[0].getparams())
    for wav in waves:
      output.writeframes(wav.readframes(wav.getnframes()))
      wav.close()
    output.close()
    buff.flush()
  except:
    for wav in waves:
      wav.close()
    if buff:    buff.close()
    if output:  output.close()
    return None # some file is missing/incorrect; Revert to generated sound
  play = subprocess.Popen(['aplay','-q'], stdin=subprocess.PIPE)
  play.stdin.write(buff.getvalue())
  buff.close()
  play.stdin.close()
  return play

def keywords(SETTING):
  keyword_list = dict()
  for mode, actions in SETTINGS.items():
    for keyword, model in actions.items():
      if keyword in keyword_list:
        continue
      keyword_list[keyword]=model['model_folder']
  return keyword_list

def learn_model(word, language, path):
  phase = INIT
  train = info[language]['train']
  try:
    keyword = info[language]['keywords'][word]
  except:
    keyword = word
  REST_data = {
    'name': word,
    'language': language,
    'microphone': 'MATRIXIO array',
    'token': TOKEN                  }
  voice = []
  for msg, sounds in zip_longest(train, train_word_wave):
    msg = msg.format(keyword)
    print("Phase {}: {}".format(phase, msg))
    
    # play prerecorded message
    sounds[1] = word
    play = play_sounds(sounds, language)

    if not play: # Use generated speach
      speak_args = espeak_cmd.format(language).split()
      speak_args.append(msg)
      speak = subprocess.Popen(speak_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
      play  = subprocess.Popen(['aplay','-q'], stdin=speak.stdout)
      speak.stdout.close()
    play.wait()
    cmd = "arecord -q -d 5 -r 16000 -f S16_LE"
    process = subprocess.Popen(cmd.split(' '),
                               stdout = subprocess.PIPE,
                               stderr = subprocess.PIPE)
    if (phase < 3):
      voice.append({'wave': base64.b64encode(process.stdout.read())})
    else:
      voice_test = base64.b64encode(process.stdout.read())
    if (phase == 2):
      REST_data["voice_samples"] = voice
      if not send: return True
      response = requests.post(endpoint, json=REST_data)
      if response.ok:
        with open(path, "wb") as model_file:
          model_file.write(response.content)
      else:
        print("Failed at phase={}".format(phase))
        return False
      # Not performming verification!!!
      break
    elif (phase > 2):
      print("Verification")
      # response = requests.post(endpoint, json=REST_data)
    phase += 1
  return True

if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('-l', '--language', required=True, choices=LANGUAGES, help='language of personal voice command models')
  parser.add_argument('-s', '--skip', help='skip models generation', action='store_true')
  parser.add_argument('-O', '--overwrite', help='overwrite current models', action='store_true')
  
  args = parser.parse_args()
  language = args.language

  if language not in LANGUAGES:
    print("Language {} not supported".format(language))
    sys.exit()
  send = not args.skip
  
  instructions = info[language]['instruct']
  keyword_list = keywords(SETTINGS)

  for keyword, folder in keyword_list.items():
    path = "saam_resources/{}/{}/{}.pmdl".format(language, folder, keyword)
    if not args.overwrite:
      if is_file(path): continue
    state = INIT
    while state < DONE:
      try:
        instruct_path = instruct_wave[state]
      except:
        instruct_path = None
      instruction = instructions[state]
      if instruction: 
        print(instruction)
        play = play_sounds(instruct_path, language)
        if not play: # No or wrong sound, revert to generated speach
          speak_args = espeak_cmd.format(language).split()
          speak_args.append(instruction)
          speak = subprocess.Popen(speak_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
          play  = subprocess.Popen(['aplay','-q'], stdin=speak.stdout)
          speak.stdout.close()
        play.wait()
      if (state == INIT):
        count = 0
        state = LEARN
      elif (state == LEARN):
        if learn_model(keyword, language, path):
          state = NEXT
        else:
          count += 1
          state = BAD
      elif (state == NEXT):
        state = DONE
      elif (state == BAD):
        if count > MAX_LEARN:
          state = NEXT
        else:
          state = LEARN
      if (state != DONE):
        time.sleep(0.500)

