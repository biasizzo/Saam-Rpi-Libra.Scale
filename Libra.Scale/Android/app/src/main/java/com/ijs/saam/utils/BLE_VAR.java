package com.ijs.saam.utils;

/*
    Created by Andraz Simcic on 22/07/2020.
*/

/**
 * Variables used for BLE EventBus messaging
 */
public class BLE_VAR {
    public static final int BLE_CONNECT = 1;
    public static final int BLE_DISCONNECT = 2;
    public static final int BLE_CONNECTING = 3;
    public static final int BLE_CONNECTED = 4;
    public static final int BLE_DISCONNECTED = 5;
    public static final int BLE_DEVICE_FOUND = 6;
    public static final int BLE_SCAN_START = 7;
    public static final int BLE_SCAN_STOP = 8;
    public static final int BLE_NOTIFICATION = 9;
    public static final int BLE_CHARA_READ = 10;
    public static final int BLE_CHARA_WRITE = 11;
    public static final int BLE_LOGIN = 12;
    public static final int BLE_SET_TARE = 13;

    public static final int BLE_SERVICE_DEVICE_FOUND = 1;
    public static final int BLE_SERVICE_CONNECTED = 2;
    public static final int BLE_SERVICE_WRONG_PASSWORD = 3;
    public static final int BLE_SERVICE_LOGIN_SUCCESS = 4;
    public static final int BLE_SERVICE_WEIGHT_NOTIFICATION = 5;
    public static final int BLE_SERVICE_BATTERY_READ = 6;
    public static final int BLE_SERVICE_DISCONNECTED = 7;
    public static final int BLE_SERVICE_SETTINGS_READ = 8;

    public static final int REQUEST_ENABLE_BT = 101;
    public static final int REQUEST_PERMISSION_LOCATION_FINE = 201;
}
