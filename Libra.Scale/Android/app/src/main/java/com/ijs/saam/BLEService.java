package com.ijs.saam;

/*
    Created by Andraz Simcic on 22/07/2020.
*/

import android.Manifest;
import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.ParcelUuid;
import android.util.Log;
import android.widget.Toast;

import androidx.annotation.Nullable;

import com.ijs.saam.msg.BLEServiceMsg;
import com.ijs.saam.msg.BLEServiceStateMsg;
import com.ijs.saam.utils.BLE_VAR;
import com.ijs.saam.utils.ScaleUUID;

import org.greenrobot.eventbus.EventBus;
import org.greenrobot.eventbus.Subscribe;
import org.greenrobot.eventbus.ThreadMode;

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.UUID;

public class BLEService extends Service {
    private static final String TAG = "BLEService";
    private static final long SCAN_PERIOD = 10000;

    public boolean isRunning = false; //true if this service is currently running
    private BluetoothManager bluetoothManager;
    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner bluetoothLeScanner;
    private ScanCallback scanCallback;
    private boolean scanning = false;
    private boolean connected = false;
    private Boolean gattBusy = false;
    private Handler handler;
    private BluetoothGatt gatt;

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        EventBus.getDefault().register(this);
        Log.d(TAG, "onCreate: BLEService");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        initialize();
        isRunning = true;
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        isRunning = false;
        EventBus.getDefault().unregister(this);
        Log.d(TAG, "onDestroy: Destroyed BLEService");
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    public void onBLEServiceMsg(BLEServiceMsg msg) {
        if (msg.cmd == BLE_VAR.BLE_CONNECT) {
            if (scanning) {
                stopScan();
            }
            Log.d(TAG, "onBLEServiceMsg: Connecting to "+msg.bluetoothDevice);
            connectDevice(msg.bluetoothDevice);
        } else if (msg.cmd == BLE_VAR.BLE_SCAN_START) {
            startScan();
        } else if (msg.cmd == BLE_VAR.BLE_LOGIN) {
            new writeTask(ScaleUUID.SERVICE_LOGIN, ScaleUUID.CHARA_PASS_IN, msg.pass).execute();
        } else if (msg.cmd == BLE_VAR.BLE_DISCONNECT) {
            disconnectGattServer();
        } else if (msg.cmd == BLE_VAR.BLE_NOTIFICATION) {
            new setNotificationTask(msg.serviceUUID, msg.characteristicUUID, msg.indicate, msg.enabled).execute();
        } else if (msg.cmd == BLE_VAR.BLE_SET_TARE) {
            new writeTask(ScaleUUID.SERVICE_CONFIG, ScaleUUID.CHARA_TARE, "0").execute();
        } else if (msg.cmd == BLE_VAR.BLE_CHARA_READ) {
            new readTask(msg.serviceUUID, msg.characteristicUUID).execute();
        }
    }

    private void startScan() {
        if (!hasPermissions() || scanning) {
            return;
        }
        Log.d(TAG, "startScan: Scan started!");
        List<ScanFilter> filters = new ArrayList<>();
        //ScanFilter scanFilter = new ScanFilter.Builder().setServiceUuid(new ParcelUuid(ScaleUUID.SERVICE_LOGIN)).build();
        //filters.add(scanFilter);
        ScanSettings settings = new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_POWER).build();
        bluetoothLeScanner = bluetoothAdapter.getBluetoothLeScanner();
        scanCallback = new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                super.onScanResult(callbackType, result);
                Log.d(TAG, "onScanResult: ");
                EventBus.getDefault().post(new BLEServiceStateMsg(BLE_VAR.BLE_SERVICE_DEVICE_FOUND, result.getDevice()));
            }

            @Override
            public void onBatchScanResults(List<ScanResult> results) {
                super.onBatchScanResults(results);
                for (ScanResult result : results) {
                    Log.d(TAG, "onScanResult: RESULT: "+result.getDevice());
                    EventBus.getDefault().post(new BLEServiceStateMsg(BLE_VAR.BLE_SERVICE_DEVICE_FOUND, result.getDevice()));
                }
            }

            @Override
            public void onScanFailed(int errorCode) {
                super.onScanFailed(errorCode);
                Log.e(TAG, "onScanFailed: " + errorCode);
            }
        };
        bluetoothLeScanner.startScan(filters, settings, scanCallback);
        scanning = true;
        handler = new Handler();
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (scanning) {
                    stopScan();
                }
            }
        }, SCAN_PERIOD);
    }

    private void stopScan() {
        if (scanning && bluetoothAdapter != null && bluetoothAdapter.isEnabled() && bluetoothLeScanner != null) {
            bluetoothLeScanner.stopScan(scanCallback);
        }
        scanCallback = null;
        scanning = false;
        handler = null;
        Log.d(TAG, "stopScan: Scan stopped!");
        EventBus.getDefault().post(new BLEServiceStateMsg(BLE_VAR.BLE_SCAN_STOP));
    }

    private void connectDevice(final BluetoothDevice bluetoothDevice) {
        BluetoothGattCallback bluetoothGattCallback = new BluetoothGattCallback() {
            @Override
            public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
                super.onConnectionStateChange(gatt, status, newState);
                synchronized (gattBusy) {
                    gattBusy = false;
                }
                if (status == BluetoothGatt.GATT_FAILURE) {
                    disconnectGattServer();
                    return;
                } else if (status != BluetoothGatt.GATT_SUCCESS) {
                    disconnectGattServer();
                    return;
                }
                if (newState == BluetoothProfile.STATE_CONNECTING) {
                    Log.d(TAG, "onConnectionStateChange: DEVICE IS CONNECTING");
                } else if (newState == BluetoothProfile.STATE_CONNECTED) {
                    //EventBus.getDefault().post(new BLEServiceStateMsg(BLE_VAR.BLE_SERVICE_CONNECTED));
                    Log.d(TAG, "onConnectionStateChange: Device connected!");
                    connected = true;
                    gatt.discoverServices();
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    disconnectGattServer();
                }
                //Log.d(TAG, "onConnectionStateChange: STATE: "+ newState);
            }

            @Override
            public void onServicesDiscovered(final BluetoothGatt gatt, int status) {
                super.onServicesDiscovered(gatt, status);
                synchronized (gattBusy) {
                    gattBusy = false;
                }
                // Log connected devices services (UUID list)
                /*Log.d(TAG, "onServicesDiscovered: SERVICES: " + gatt.getServices());
                for (BluetoothGattService service : gatt.getServices()) {
                    Log.d(TAG, "onServicesDiscovered: SERVICE: " + service.getUuid());
                }*/
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    return;
                } else {
                    EventBus.getDefault().post(new BLEServiceStateMsg(BLE_VAR.BLE_SERVICE_CONNECTED));
                }
            }

            @Override
            public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
                super.onCharacteristicWrite(gatt, characteristic, status);
                synchronized (gattBusy) {
                    gattBusy = false;
                }
                Log.d(TAG, "onCharacteristicWrite: PASS STATUS = " + status);
                if(characteristic.getUuid().equals(ScaleUUID.CHARA_PASS_IN) && status != 0) {
                    // if wrong password
                    Log.d(TAG, "onCharacteristicWrite: WRONG PASSWORD status: " + status);
                    EventBus.getDefault().post(new BLEServiceStateMsg(BLE_VAR.BLE_SERVICE_WRONG_PASSWORD));
                } else if (characteristic.getUuid().equals(ScaleUUID.CHARA_PASS_IN) && status == 0) {
                    EventBus.getDefault().post(new BLEServiceStateMsg(BLE_VAR.BLE_SERVICE_LOGIN_SUCCESS));
                }
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    Log.d(TAG, "onCharacteristicWrite: Successfully written data to scale!");
                }
            }

            @Override
            public void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
                super.onCharacteristicRead(gatt, characteristic, status);
                synchronized (gattBusy) {
                    gattBusy = false;
                }
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    Log.d(TAG, "onCharacteristicRead: Successfully read data to scale!");
                    byte[] messageBytes = characteristic.getValue();
                    Log.d(TAG, "onCharacteristicRead: VALUE "+messageBytes.toString());
                    String messageString = null;
                    try {
                        messageString = new String(messageBytes, "UTF-8");
                    } catch (UnsupportedEncodingException e) {
                        Log.e(TAG, "Unable to convert message bytes to string");
                    }
                    Log.d(TAG, "Received message: " + messageString);
                    if(characteristic.getUuid().equals(ScaleUUID.CHARA_BATTERY_CUSTOM)) {
                        EventBus.getDefault().post(new BLEServiceStateMsg(BLE_VAR.BLE_SERVICE_BATTERY_READ, messageString));
                    } else if(characteristic.getUuid().equals(ScaleUUID.CHARA_GET_CUR_SETTINGS)) {
                        EventBus.getDefault().post(new BLEServiceStateMsg(BLE_VAR.BLE_SERVICE_SETTINGS_READ, messageString));
                    }
                }
            }

            @Override
            public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
                super.onCharacteristicChanged(gatt, characteristic);
                synchronized (gattBusy) {
                    gattBusy = false;
                }
                Log.d(TAG, "onCharacteristicChanged gatt: " + gatt + " characteristic: " + characteristic);
                if (characteristic.getUuid().equals(ScaleUUID.CHARA_WEIGHT)) {
                    byte[] messageBytes = characteristic.getValue();
                    String messageString = null;
                    try {
                        messageString = new String(messageBytes, "UTF-8");
                    } catch (UnsupportedEncodingException e) {
                        Log.e(TAG, "Unable to convert message bytes to string");
                    }
                    //Log.d(TAG, "WEIGHT: " + messageString);
                    EventBus.getDefault().post(new BLEServiceStateMsg(BLE_VAR.BLE_SERVICE_WEIGHT_NOTIFICATION, messageString));
                }
            }



            @Override
            public void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
                super.onDescriptorWrite(gatt, descriptor, status);
                synchronized (gattBusy) {
                    gattBusy = false;
                }
            }
        };
        gatt = bluetoothDevice.connectGatt(this, false, bluetoothGattCallback);
        gatt.discoverServices();
    }

    public void disconnectGattServer() {
        if(connected) {
            connected = false;
            if (gatt != null) {
                gatt.disconnect();
                gatt.close();
            }
            Log.d(TAG, "disconnectGattServer: Device disconnected!");
            EventBus.getDefault().post(new BLEServiceStateMsg(BLE_VAR.BLE_SERVICE_DISCONNECTED));
            // Toast done on main thread
            //Toast.makeText(getApplicationContext(), "Povezava s tehtnico prekinjena!", Toast.LENGTH_SHORT).show();
        }
    }

    /**
     *
     * @return true if successfully initialized and false otherwise
     */
    private boolean initialize() {
        Log.d(TAG, "initialize");
        if (bluetoothManager == null) {
            bluetoothManager = (BluetoothManager) getSystemService(BLUETOOTH_SERVICE);
            if(bluetoothManager == null) {
                Log.e(TAG, "initialize: Unable to initialize bluetoothManager!");
                return false;
            }
        }
        bluetoothAdapter = bluetoothManager.getAdapter();
        if (bluetoothAdapter == null) {
            Log.e(TAG, "initialize: Unable to initialize bluetoothAdapter!");
            return false;
        }
        return true;
    }

    private BluetoothGattCharacteristic getCharacteristic (UUID serviceUUID, UUID characteristicUUID) throws Exception{
        if (gatt != null) {
            BluetoothGattService service = gatt.getService(serviceUUID);
            if (service != null) {
                BluetoothGattCharacteristic characteristic = service.getCharacteristic(characteristicUUID);
                if (characteristic != null) {
                    return characteristic;
                } else {
                    throw new Exception("getCharacteristic: Can't access characteristic with UUID: " + characteristicUUID.toString());
                }
            } else {
                throw new Exception("getCharacteristic: Can't access service with UUID: " + serviceUUID.toString());
            }
        } else {
            throw new Exception("getCharacteristic: bluetoothGatt is null");
        }
    }

    private class setNotificationTask extends AsyncTask<Void,Void,Void> {
        UUID serviceUUID;
        UUID characteristicUUID;
        boolean indicate;
        boolean enabled;

        /**
         * Enables or disables notification on a give characteristic.
         *
         * @param serviceUUID The GATT service UUID containing desired characteristic
         * @param characteristicUUID The GATT characteristic UUID to read from
         * @param indicate If true set indicate, if false set notify
         * @param enabled If true, enable notification.  False otherwise.
         */
        setNotificationTask(UUID serviceUUID, UUID characteristicUUID,
                            boolean indicate, boolean enabled) {
            this.serviceUUID = serviceUUID;
            this.characteristicUUID = characteristicUUID;
            this.indicate = indicate;
            this.enabled = enabled;
        }

        @Override
        protected Void doInBackground(Void... params) {
            Log.d(TAG, "setNotificationTask");
            if (bluetoothAdapter == null || gatt == null) {
                Log.w(TAG, "BluetoothAdapter not initialized");
                return null;
            }
            while (gattBusy) {
                try {
                    Thread.currentThread().sleep(100);
                } catch (Exception e) {Log.e(TAG, e.getMessage()); }
            } //wait till its free
            BluetoothGattCharacteristic characteristic;
            try {
                characteristic = getCharacteristic(serviceUUID, characteristicUUID);
            } catch (Exception e) {
                Log.e(TAG, "setNotificationTask" + e.getMessage());
                return null;
            }

            gatt.setCharacteristicNotification(characteristic, enabled);

            BluetoothGattDescriptor descriptor = characteristic.getDescriptor(ScaleUUID.DESCRIPTOR_NOTIFICATION_ID);
            if (indicate)
                descriptor.setValue(BluetoothGattDescriptor.ENABLE_INDICATION_VALUE);
            else
                descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);

            synchronized (gattBusy) {
                gattBusy = true;
            }
            gatt.writeDescriptor(descriptor);
            return null;
        }
    }


    public class writeTask extends AsyncTask<Void, Void, Void> {
        UUID serviceUUID;
        UUID characteristicUUID;
        String value;
        byte[] bytes;

        /**
         * Request a write on a given {@code BluetoothGattCharacteristic}. The read result is reported
         * asynchronously through the {@code BluetoothGattCallback#onCharacteristicWrite(android.bluetooth.BluetoothGatt, android.bluetooth.BluetoothGattCharacteristic, int)}
         * callback.
         *
         * @param serviceUUID The GATT service UUID containing desired characteristic
         * @param characteristicUUID The GATT characteristic UUID to read from
         * @param value The String value to be written
         *
         **/
        public writeTask(UUID serviceUUID, UUID characteristicUUID, String value) {
            Log.d(TAG, "writeTask string");
            this.serviceUUID = serviceUUID;
            this.characteristicUUID = characteristicUUID;
            if (value != null)
                this.value = value;
            else
                this.value = "";
        }

        /**
         * Request a write on a given {@code BluetoothGattCharacteristic}. The read result is reported
         * asynchronously through the {@code BluetoothGattCallback#onCharacteristicWrite(android.bluetooth.BluetoothGatt, android.bluetooth.BluetoothGattCharacteristic, int)}
         * callback.
         *
         * @param serviceUUID The GATT service UUID containing desired characteristic
         * @param characteristicUUID The GATT characteristic UUID to read from
         * @param bytes The bytes to be written
         *
         **/
        public writeTask(UUID serviceUUID, UUID characteristicUUID, byte[] bytes) {
            Log.d(TAG, "writeTask bytes");
            this.serviceUUID = serviceUUID;
            this.characteristicUUID = characteristicUUID;
            if (bytes != null)
                this.bytes = bytes;
            else
                this.bytes = new byte[]{};
        }

        @Override
        protected Void doInBackground(Void... params) {
            Log.d(TAG, "writeTask characteristic UUID: " + characteristicUUID.toString());
            if (value != null)
                Log.d(TAG, "writeTask characteristic UUID: " + characteristicUUID.toString() + " value: " + value);
            else
                Log.d(TAG, "writeTask characteristic UUID: " + characteristicUUID.toString() + " bytes:" + Arrays.toString(bytes));

            if (bluetoothAdapter == null || gatt == null) {
                Log.w(TAG, "BluetoothAdapter not initialized");
                return null;
            }
            if (value != null) {
                if (value.length() > 20)
                    value = value.substring(0, 20);
            }
            if (bytes != null) {
                if (bytes.length > 20) {
                    bytes = Arrays.copyOf(bytes, 20);
                }
            }

            while (gattBusy) {
                Log.d(TAG, "doInBackground: WRITE LOCKED: "+gattBusy);
                try {
                    Thread.currentThread().sleep(100);
                } catch (Exception e) {Log.e(TAG, e.getMessage()); }
            } //wait till its free
            BluetoothGattCharacteristic characteristic;
            try {
                characteristic = getCharacteristic(serviceUUID, characteristicUUID);
            } catch (Exception e) {
                Log.e(TAG, "writeCharacteristic" + e.getMessage());
                return null;
            }
            if (characteristic == null) {
                Log.e(TAG, "writeCharacteristic == null");
                return null;
            }
            if (value != null) {
                if (!characteristic.setValue(value)) {
                    Log.e(TAG, "writeCharacteristic setValue string unsuccessful");
                    return null;
                }
            } else if (bytes != null) {
                if (!characteristic.setValue(bytes)) {
                    Log.e(TAG, "writeCharacteristic setValue byte array unsuccessful");
                    return null;
                }
            }

            synchronized (gattBusy) {
                gattBusy = true;
            }

            if (!gatt.writeCharacteristic(characteristic)) {
                Log.e(TAG, "writeCharacteristic writeCharacteristic unsuccessful");
                return null;
            }
            return null;
        }
    }

    public class readTask extends AsyncTask<Void,Void,Void> {
        UUID serviceUUID;
        UUID characteristicUUID;

        /**
         * Request a read on a given {@code BluetoothGattCharacteristic}. The read result is reported
         * asynchronously through the {@code BluetoothGattCallback#onCharacteristicRead(android.bluetooth.BluetoothGatt, android.bluetooth.BluetoothGattCharacteristic, int)}
         * callback.
         *
         * @param serviceUUID The GATT service UUID containing desired characteristic
         * @param characteristicUUID The GATT characteristic UUID to read from
         */
        public readTask(UUID serviceUUID, UUID characteristicUUID) {
            this.serviceUUID = serviceUUID;
            this.characteristicUUID = characteristicUUID;
        }

        @Override
        protected Void doInBackground(Void... params) {
            Log.d(TAG, "readTask");
            if (bluetoothAdapter == null || gatt == null) {
                Log.w(TAG, "BluetoothAdapter not initialized");
                return null;
            }

            while (gattBusy) {
                try {
                    Thread.currentThread().sleep(100);
                } catch (Exception e) {Log.e(TAG, e.getMessage()); }
            } //wait till its free

            BluetoothGattCharacteristic characteristic;
            try {
                characteristic= getCharacteristic(serviceUUID, characteristicUUID);
            } catch (Exception e) {
                Log.e(TAG, "readCharacteristic" + e.getMessage());
                return null;
            }

            synchronized (gattBusy) {
                gattBusy = true;
            }

            gatt.readCharacteristic(characteristic);
            return null;
        }
    }

    /**
     *
     * @return true if both bluetooth is on and permissions are granted
     */
    private boolean hasPermissions() {
        if(checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_DENIED) {
            return false;
        }
        if (!bluetoothAdapter.isEnabled()) {
            return false;
        }
        return true;
    }
}
