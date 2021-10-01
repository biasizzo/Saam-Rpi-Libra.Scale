**This is the official repository for SAAM SNLI.**

Author: Vid Podpecan (vid.podpecan@ijs.si)

# Installation instructions

## Software requirements

The following system packages are required. Use `sudo apt install <package_name>` to install every package on the list (if it is not already installed):

- python3 (3.5 is recommended)
- espeak-ng
- libsndfile1

The following python3 packages are required. They need to be installed using `pip3` (see section **Environment setup**). They are also listed in the `requirements.txt` file in the repository.

- numpy==1.16.4
- paho-mqtt==1.4.0
- py-espeak-ng==0.1.8
- pytz==2019.1
- sounddevice==0.3.13
- SoundFile==0.10.2


## Hardware setup and system configuration

The default hardware platform for SNLI is Raspberry Pi 3 and the default microphone module is Matrix Creator. The target Raspberry Pi 3 should run the latest edition of the Raspbian system. Python 3 is required (version 3.5 is recommended). The hardware setup procedure for the Matrix Creator is described in the official manual and is available online [here](https://matrix-io.github.io/matrix-documentation/matrix-creator/device-setup/) . For SNLI, only the microphone setup for the Matrix Creator is mandatory (see [here](https://matrix-io.github.io/matrix-documentation/matrix-creator/resources/microphone/) for details).

Note that is is important that the Matrix Creator microphone array is the default audio recording device. This can be accomplished by using the ALSA configuration file (`asound.conf`) provided in the official Matrix source repository [here](https://raw.githubusercontent.com/matrix-io/matrixio-kernel-modules/master/misc/asound.conf). Copy the content of this file into the local file `~/.asoundrc` and restart the computer. 
To test that the Matrix Creator microphone array is configured correctly, the following commands should work:

`arecord -l ` should return an output similar to the one listed below where the device `MATRIXIO SOUND` can be identified:

```**** List of CAPTURE Hardware Devices ****
card 1: Dummy [Dummy], device 0: Dummy PCM [Dummy PCM]
  Subdevices: 8/8
  Subdevice #0: subdevice #0
  Subdevice #1: subdevice #1
  Subdevice #2: subdevice #2
  Subdevice #3: subdevice #3
  Subdevice #4: subdevice #4
  Subdevice #5: subdevice #5
  Subdevice #6: subdevice #6
  Subdevice #7: subdevice #7
card 2: SOUND [MATRIXIO SOUND], device 0: matrixio.mic.0 snd-soc-dummy-dai-0 []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
```

`arecord -f cd test.wav` should record the audio in CD quality into the `test.wav` file.

`aplay test.wav` should play the recorded audio.


## Downloading SNLI

The SNLI implementation is available in a Git archive hosted at Bitbucket. The code can be downloaded to the local machine using the following command:

`git clone git@bitbucket.org:vpodpecan/saam-snli.git`

This will produce a directory named `saam-snli` which contains the SNLI code, the Snowboy library and resources compiled for Raspberry Pi and a directory structure called `saam-resources`  where the trained voice command models will reside (each language has a separate directory).


## Environment setup

The SNLI can be run using system's python packages or it can be run inside a python virtual environment. If you want a virtual environment, the following commands will set it up:

`python3 -m venv ~/saam_venv`


The newly created `saam_venv` can be activated by typing

`source ~/saam_venv/bin/activate`

Alternatively, the environment's python can be invoked directly by running

`~/saam_venv/bin/python3`

which is very useful for running the main SNLI script automatically when the system boots up.

Finally, you need to install the required python packages for SNLI:

- using a virtual environment:
 `~/saam_venv/bin/pip3 install -r requirements.txt`
- using system python and packages:
 `pip3 install -r requirements.txt`
 

## Hotword model training and installation

SNLI requires trained personal Snowboy models. The easiest way to obtain them is to use the Snowboy’s web interface which is available [here](https://snowboy.kitt.ai/dashboard) (registration is required).

By clicking the "Create hotword" button and following the instructions a personal hotword model can be built and downloaded. Once you have the model saved locally, place it in the appropriate language subfolder in the `saam-snli/saam-resources/...` folder and repeat the process for all the required hotwords. You need to record and download models for the following hotwords or phrases:

- wake up word --> folder `saam_resources/bg/keyword_saam`
- call for help --> folder `saam_resources/bg/keyword_help`
- call mode activation --> folder `saam_resources/bg/keyword_call`
- call the neighbour --> folder `saam_resources/bg/keyword_neighbour`
- call the family --> folder`saam_resources/bg/keyword_family`
- stop the alert mode --> folder `saam_resources/bg/keyword_saam_stop`

Using the English language, the following hotwords are used: **saam, help, call, neighbour, family, saam stop**. Note that for any supported language you can use any spoken phrase for any of the keywords as long as they are stored in the appropriate folder. For example, if you want to give a name to your Bulgarian SNLI instance, you can record the model with the selected name and put it into the folder `saam_resources/bg/keyword_saam`. Then you can use this name to wake up the system (see the next section).



## SNLI configuration

In order to run the current SNLI workflow (see section **Using SNLI**) in English, Slovene, German or Bulgarian no modifications are required if the language models are stored in the appropriate folders.

Currently, a basic set of response texts for synthesized speech output is provided for English, Slovene, German and Bulgarian but you may want to modify them to suit your needs. You can do that by editing the `saam_reponses.py` file. For example, when SNLI recognizes the phrase for the **help** command, the following configuration applies:

```
HELP: {'en': 'I am calling for help!',
       'sl': 'Kličem pomoč!',
       'de': 'Ich rufe um Hilfe!',
       'bg': 'Призовавам за помощ!'
},
```
By modifying the text in quotes you can modify SNLI's speech output for any of the supported languages. Note that SNLI will work without configured response messages but an error message will be printed.



## Running SNLI

Once the language models are placed in the corresponding folders, the SNLI for the selected language can be launched as follows:

`python3 snli.py -l en` will launch SNLI for the English language.

There are also the following optional parameters:
- `-s` which allows setting the hotword recognition sensivity (0.5 by default). Higher value means higher sensivity and potentially more false alarms while lower value means lower sensivity and potentially missed spoken commands.
- `-t` which allows setting the timeout for keyword repetition. By default, SNLI will wait 5 seconds for the repeated command to be recognized before cancelling the action.
- `-r` which allows setting the number of required keyword repetitions. By default, 2 repetitions are requested.
- `-v` enables verbose operation mode. This mode is for debugging purposes.


## Using SNLI

The SNLI workflow defines 4 system modes: **background**, **active**,  **call**, and **alert**. The system makes transitions between them according to the recognized keyword and a built-in timer which returns the system into the background mode after a short delay (an exception is the **alert** mode which stays active until the **saam stop** command is recognized). Every keywords has to be recognized a specified number of times (twice by default) inside a given time window (5 seconds by default) in order to be taken into account. This prevents many false detections and improves robustness.


### Background mode

The default behaviour of SNLI is to start in the background mode and wait for the keyword `saam` which puts SNLI into the active mode, or keyword `help`, which puts the system into the alert mode.


### Active mode 

The active mode supports two commands: `call` and `help`. The `call` keyword will put the system into the call mode. The `help` keyword will put the system into the alert mode. The active mode will exit into the background mode if no keyword is recognized within the time window.


### Call mode

The call mode supports two keywords: `family` and `neighbour`. The `family` keyword will place a call to the stored phone number of the selected family member while the `neighbour` keyword will place a call to the stored number of the selected neighbour.


### Alert mode

The alert mode is provided as a special mode which handles events that require immediate attention. Currently, the mode can be activated from the background and active modes by using the keyword `help`. Once activated, the alert mode will start a stored sequence of actions as defined for the specific event, e.g., call for an ambulance. The alert mode will stay active until it is deactivated by the `saam stop` command.
