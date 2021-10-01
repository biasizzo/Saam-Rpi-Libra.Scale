package com.ijs.saam.msg;

import android.bluetooth.BluetoothDevice;

/*
    Created by andra on 22/07/2020.
*/
public class BLEServiceStateMsg {
    public int state;
    public BluetoothDevice bluetoothDevice;
    public String s1;

    public BLEServiceStateMsg(int state, BluetoothDevice bluetoothDevice) {
        this.state = state;
        this.bluetoothDevice = bluetoothDevice;
        this.s1 = null;
    }

    public BLEServiceStateMsg(int state) {
        this.state = state;
        bluetoothDevice = null;
        this.s1 = null;
    }

    public BLEServiceStateMsg(int state, String weight) {
        this.state = state;
        bluetoothDevice = null;
        this.s1 = weight;
    }
}
