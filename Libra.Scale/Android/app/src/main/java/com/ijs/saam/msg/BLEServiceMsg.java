package com.ijs.saam.msg;

/*
    Created by Andraz Simcic on 22/07/2020.
*/

import android.bluetooth.BluetoothDevice;

import java.util.UUID;

public class BLEServiceMsg {
    public int cmd;
    public UUID serviceUUID;
    public UUID characteristicUUID;
    public BluetoothDevice bluetoothDevice;
    public String pass;
    public boolean indicate;
    public boolean enabled;


    public BLEServiceMsg(int cmd) {
        this.cmd = cmd;
        this.serviceUUID = null;
        this.characteristicUUID = null;
        this.bluetoothDevice = null;
        this.pass = null;
        this.indicate = false;
        this.enabled = false;
    }

    public BLEServiceMsg(int cmd, UUID serviceUUID, UUID characteristicUUID, boolean indicate, boolean enabled) {
        this.cmd = cmd;
        this.serviceUUID = serviceUUID;
        this.characteristicUUID = characteristicUUID;
        this.bluetoothDevice = null;
        this.pass = null;
        this.indicate = indicate;
        this.enabled = enabled;
    }

    public BLEServiceMsg(int cmd, UUID serviceUUID, UUID characteristicUUID) {
        this.cmd = cmd;
        this.serviceUUID = serviceUUID;
        this.characteristicUUID = characteristicUUID;
        this.bluetoothDevice = null;
        this.pass = null;
        this.indicate = false;
        this.enabled = false;
    }

    public BLEServiceMsg(int cmd, BluetoothDevice bluetoothDevice) {
        this.cmd = cmd;
        this.serviceUUID = null;
        this.characteristicUUID = null;
        this.bluetoothDevice = bluetoothDevice;
        this.pass = null;
        this.indicate = false;
        this.enabled = false;
    }

    public BLEServiceMsg(int cmd, String pass) {
        this.cmd = cmd;
        this.serviceUUID = null;
        this.characteristicUUID = null;
        this.bluetoothDevice = null;
        this.pass = pass;
        this.indicate = false;
        this.enabled = false;
    }
}
