# LIBRA SCALE 2.0 FIRMWARE

Developed within TETRAMAX bilatral technology transfer project BALLERINA.



## Install


- Download and extract nRF52 SDK 15.2.0
- Clone project into SDKDIR\examples\ble_peripheral\custom_ble_service_tehtnica

- Linker scripts, flash placement is set up for segger project:
```
SDKDIR\examples\ble_peripheral\custom_ble_service_tehtnica\pca10040\s132\ses\ble_app_blinky_pca10040_s132.emProject
```


## Information


The firmware is composed of the following:
* application - Scale Firmware project
* bootloader  - Scale Bootloader project
* softdevice  - Closed source BLE API developed by Nordic Semiconductor for NRF52 SDK.

### 1. Flashing firmware
Required: nrfutil, nRF Command line tools, application and bootloader binaries.

#### 1.1 Flashing firmware for production

Generate settings and merge to application binary:
```
nrfutil settings generate --family NRF52QFAB --application "ble_app_tehtnica_pca10040_s132.hex" --application-version 1 --bootloader-version 1 --bl-settings-version 1 "bootloader_settings.hex"
mergehex -m "ble_app_tehtnica_pca10040_s132.hex" "bootloader_settings.hex" -o "merged.hex"
```

First, erase:
```
nrfjprog --family NRF52 --eraseall
```

Program softdevice:
```
nrfjprog --family NRF52 --program "SDKDIR\components\softdevice\s132\hex\s132_nrf52_6.1.0_softdevice.hex"
```

Program bootloader:
```
nrfjprog --family NRF52 --program "secure_bootloader_tehtnica_ble_s132_pca10040.hex"
```

Program app+settings (from previous step)
```
nrfjprog --family NRF52 --program "merged.hex"
nrfjprog --family NRF52 --reset
```

Alternatively all of the above hex files can be merged to a single file using mergehex and flashed in one go. Similarly settings can be flashed seperately or merged to any other binary.


#### 1.2 Flashing firmware for debugging
In order to make the application run without a bootloader present, comment these two lines in main()
```
//ret_code_t err_code = ble_dfu_buttonless_async_svci_init();
//APP_ERROR_CHECK(err_code);
```

App can now be flashed and debugged from IDE.


### 2. OTA update procedure

This project uses Buttonless DFU (Device Firmware Update) for bluetooth OTA updates.
Scale Firmware switches to bootloader (using buttonless method) when DFU request is received.
Bootloader performs the update. A signed zip is sent to the bootloader and checked against the public key embedded in bootloader firmware.

#### 2.1 Generating new keys (optional)
Required: nrfutil
```
nrfutil keys generate private.key
nrfutil keys display --key pk --format code private.key --out_file public_key.c
```
Replace public_key.c in the bootloader project. 

#### 2.2 Generating signed zip to use with OTA update
Required: nrfutil, private key, application binary
```
nrfutil pkg generate --hw-version 52 --application-version 2 --application "ble_app_tehtnica_pca10040_s132.hex" --sd-req 0xAF --key-file private.key app_dfu_package.zip
```
* Note: Don't merge settings to this application binary.
* Note: application-version must be greater than the one this zip is replacing.


#### 2.3 Performing DFU/OTA update

Update can be performed by nRF Connect app for android:
* Connect to the scale
* Click DFU icon in top right corner (Icon is only shown for devices advertising the correct DFU characteristics)
* Select the zip
* Done!

BLE DFU can also be performed by:
* nRF connect for desktop
* nrfutil command line utility
Note: We have not tested these.


#### 2.4 Links to libraries implementing Buttonless DFU for nRF52:
* [Android DFU Library](https://github.com/NordicSemiconductor/Android-DFU-Library)
* [IOS DFU Library](https://github.com/NordicSemiconductor/IOS-Pods-DFU-Library)

