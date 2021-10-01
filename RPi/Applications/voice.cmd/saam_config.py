import os
import sys

from saam_callbacks import SAAM
from saam_constants import BACKGROUND_MODE, ACTIVE_MODE, ALERT_MODE, CALL_MODE, MODEL_FOLDER, CALLBACK


SETTINGS = {BACKGROUND_MODE: {'saam': {MODEL_FOLDER: 'keyword_saam', CALLBACK: SAAM.ok_saam},
                              'help' : {MODEL_FOLDER: 'keyword_help', CALLBACK: SAAM.help}
                             },
            ACTIVE_MODE: {'call': {MODEL_FOLDER: 'keyword_call', CALLBACK: SAAM.call},
                          'help': {MODEL_FOLDER: 'keyword_help', CALLBACK: SAAM.help}
                         },
            ALERT_MODE: {'saam stop': {MODEL_FOLDER: 'keyword_saam_stop', CALLBACK: SAAM.saam_stop}
                        },
            CALL_MODE: {'family': {MODEL_FOLDER: 'keyword_family', CALLBACK: SAAM.call_family},
                        'neighbour': {MODEL_FOLDER: 'keyword_neighbour', CALLBACK: SAAM.call_neighbour},
                       }
            }


def get_languages():
    langs = []
    for x in os.listdir(os.path.abspath('saam_resources')):
        if os.path.isdir(os.path.abspath(os.path.join('saam_resources', x))):
            langs.append(x)
    return langs
            
   
SUPPORTED_LANGS = get_languages()  #list(SETTINGS.keys())


def model_files_ok(language):
    allOK = True
    for mode in SETTINGS:
        for keyword in SETTINGS[mode]:
            try:
                model_folder = os.path.join('saam_resources', language, keyword)
                _ = get_model_file(model_folder)
            except IOError:
                allOK = False
                sys.stderr.write('Error: exactly one .pmdl file is required for language "{}", mode "{}", keyword "{}" in "{}".\n'.format(language, mode, keyword, model_folder)) 
    return allOK

    
def get_model_file(model_folder):
    folderpath = os.path.abspath(model_folder)
    for dirpath, dirnames, filenames in os.walk(folderpath):
        fnames = [fn for fn in filenames if fn.lower().endswith('.pmdl')]
        if len(fnames) != 1:
            raise IOError('Invalid number of keyword model files in folder "{}".\nExactly one .pmdl file is allowed.'.format(folderpath))
        fullname = os.path.abspath(os.path.join(dirpath, fnames[0]))
        return fullname
        

def get_models_callbacks(language, mode):
    models = []
    callbacks = []
    for keyword in SETTINGS[mode]:
        model = get_model_file(os.path.join('saam_resources', language, SETTINGS[mode][keyword][MODEL_FOLDER]))
        models.append(model)
        callbacks.append(SETTINGS[mode][keyword][CALLBACK])
    return models, callbacks
