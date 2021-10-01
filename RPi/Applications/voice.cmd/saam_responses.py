# -*- coding: UTF-8 -*-
import sys

from saam_constants import OK_SAAM, HELP, STOP, HELP_REPEAT, CALL, ERROR, CALL_FAMILY, CALL_NEIGHBOUR, REPEAT_COMMAND


SAAM_RESPONSES = {
    OK_SAAM: {'en': 'I am active.',
              'sl': 'Sem aktiven.',
              'de': 'Ich bin aktiv.',
              'bg': 'Аз съм активен.'
             },
    HELP: {'en': 'I am calling for help!',
           'sl': 'Kličem pomoč!',
           'de': 'Ich rufe um Hilfe!',
           'bg': 'Призовавам за помощ!'
          },
    HELP_REPEAT: {'en': 'Please keep calm. Help is on the way!',
                  'sl': 'Prosimo, potrpite. Pomoč je na poti!',
                  'de': 'Bitte bleib ruhig. Hilfe ist auf dem Weg!',
                  'bg': 'Моля, запазете спокойствие. Помощта идва!'
                 },
    STOP: {'en': 'I am listening for wake word.',
           'sl': 'Čakam na ključno besedo.',
           'de': 'Ich höre auf das Wachwort.',
           'bg': 'Слушам за будна дума.'
          },
    CALL: {'en': 'Family or neighbour.',
           'sl': 'Družina ali sosed?',
           'de': 'Familie oder Nachbarn.',
           'bg': 'Семейство или съсед.'
          }, 
    CALL_FAMILY: {'en': 'I am calling your family.',
                  'sl': 'Kličem vašo družino.',
                  'de': 'Ich rufe deine Familie an.',
                  'bg': 'Обаждам семейството ви.'
                 },
    CALL_NEIGHBOUR: {'en': 'I am calling your neighbour.',
                    'sl': 'Kličem soseda.',
                    'de': 'Ich rufe deinen Nachbarn an.',
                    'bg': 'Обаждам съседа ви.'
                    },
    ERROR: {'en': 'The error will be reported.',
            'sl': 'Upravljalec bo obveščen o napaki.',
            'de': 'Der Fehler wird gemeldet.',
            'bg': 'Грешката ще бъде съобщена.'
           },
    REPEAT_COMMAND: {'en': 'Please repeat command.',
                     'sl': 'Prosim ponovite besedo.',
                     'de': 'Bitte Befehl wiederholen.',
                     'bg': 'Моля, повторете командата.'
                    }
    }


def get_response(command, language):
    if command not in SAAM_RESPONSES:
        sys.stderr.write('Error: command "{}" not known\n'.format(command))
        return ''
    elif language not in SAAM_RESPONSES[command]:
        sys.stderr.write('Error: command "{}" not defined for language {}\n'.format(command, language))
        return ''
    else:
        return SAAM_RESPONSES[command][language]