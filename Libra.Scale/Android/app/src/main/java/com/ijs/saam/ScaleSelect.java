package com.ijs.saam;

/*
    Created by Andraz Simcic on 22/07/2020.
*/

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

import com.ijs.saam.msg.BLEServiceMsg;
import com.ijs.saam.msg.BLEServiceStateMsg;
import com.ijs.saam.utils.BLE_VAR;

import org.greenrobot.eventbus.EventBus;
import org.greenrobot.eventbus.Subscribe;
import org.greenrobot.eventbus.ThreadMode;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;

public class ScaleSelect extends AppCompatActivity {
    private static final String TAG = "ScaleSelect";
    private String PAIRED_DEVICE_NAME = "paired_device_name";
    private String PAIRED_DEVICE_PASSWORD = "paired_device_password";

    private RecyclerView rvDeviceList;
    private SwipeRefreshLayout swipeView;
    private BluetoothManager mBluetoothManager;
    private BluetoothAdapter mBluetoothAdapter;
    private DeviceListAdapter mDeviceListAdapter;
    public ArrayList<BluetoothDevice> mBLEDevices = new ArrayList<>();
    private BLEService mBLEService;
    private String pairedDeviceAddress;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_scale_select);
        EventBus.getDefault().register(this);

        pairedDeviceAddress = loadStringFromStorage(PAIRED_DEVICE_NAME);

        rvDeviceList = (RecyclerView)findViewById(R.id.rvScales);
        swipeView = (SwipeRefreshLayout)findViewById(R.id.srlScales);
        swipeView.setOnRefreshListener(new SwipeRefreshLayout.OnRefreshListener() {
            @Override
            public void onRefresh() {
                // check for bluetooth on and permissions
                if (!mBluetoothAdapter.isEnabled()) {
                    requestBluetoothEnable();
                    swipeView.setRefreshing(false);
                } else if (!hasLocationPermissions()) {
                    requestLocationPermission();
                    swipeView.setRefreshing(false);
                } else {
                    // start BLE device scan
                    EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_SCAN_START));
                }
            }
        });
        // check if the device supports BLE
        if (!getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
            Toast.makeText(getApplicationContext(), "Device does not support Bluetooth LE.", Toast.LENGTH_SHORT).show();
            // if device does not support BLE close the application after displaying the issue
            new Handler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    finish();
                }
            }, 1000);
        }

        // start BLEService
        startBLEService();

        // init bluetooth adapter and manager
        // needed for requesting permissions from user
        mBluetoothManager = (BluetoothManager) getSystemService(BLUETOOTH_SERVICE);
        mBluetoothAdapter = mBluetoothManager.getAdapter();

        // check if bluetooth is on, if not request to turn it on
        if (!mBluetoothAdapter.isEnabled()) {
            requestBluetoothEnable();
        } else {
            if (!hasLocationPermissions()) {
                requestLocationPermission();
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.d(TAG, "onResume: RESUMED");
        startBLEService();

        // Starts discovery once all the conditions are met
        // starts it every time you open the app
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                while (!hasLocationPermissions() && !mBluetoothAdapter.isEnabled()) {
                    try {
                        wait(100);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
                EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_SCAN_START));
                swipeView.setRefreshing(true);
                try {
                    this.finalize();
                } catch (Throwable throwable) {
                    throwable.printStackTrace();
                }
            }
        });
    }

    @Override
    protected void onStop() {
        super.onStop();
        Log.d(TAG, "onStop: STOPPED");
        stopBLEService();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        EventBus.getDefault().unregister(this);
        stopBLEService();
        Log.d(TAG, "onDestroy: destroyed!");
    }

    // Subscribe to EventBus BLEServiceStateMsg
    // do different things depending on the msg state
    @Subscribe(threadMode = ThreadMode.MAIN)
    public void onBLEServiceStateMsg(BLEServiceStateMsg msg) {
        Log.d(TAG, "onBLEServiceStateMsg: STATE: "+msg.state);
        if (msg.state == BLE_VAR.BLE_SERVICE_DEVICE_FOUND) {
            if (!mBLEDevices.contains(msg.bluetoothDevice) && msg.bluetoothDevice.getName()!=null && msg.bluetoothDevice.getName().toLowerCase().contains("libra")) {
                Log.d(TAG, "onBLEServiceStateMsg: new device found "+msg.bluetoothDevice.getName());
                // add device to list
                mBLEDevices.add(msg.bluetoothDevice);

                // refresh DeviceListAdapter
                mDeviceListAdapter = new DeviceListAdapter(getApplicationContext(), mBLEDevices);
                rvDeviceList.setAdapter(mDeviceListAdapter);
                rvDeviceList.setLayoutManager(new LinearLayoutManager(ScaleSelect.this));
                mDeviceListAdapter.setOnItemClickListener(new DeviceListAdapter.ClickListener() {
                    @Override
                    public void onScaleClick(final int position, View v) {
                        Log.d(TAG, "onScaleClick: " + mBLEDevices.get(position).getName());
                        // connect to device and wait for service state CONNECTED
                        EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_CONNECT, mBLEDevices.get(position)));
                        saveStringToStorage(PAIRED_DEVICE_NAME, mBLEDevices.get(position).getAddress());
                    }
                });
                if(pairedDeviceAddress != null) {
                    if (msg.bluetoothDevice.getAddress().equals(pairedDeviceAddress)) {
                        EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_CONNECT, msg.bluetoothDevice));
                    }
                }
            }
        } else if (msg.state == BLE_VAR.BLE_SERVICE_CONNECTED) {
            // if there is a paired device, use the saved password for that device
            final String pairedDevicePass = loadStringFromStorage(PAIRED_DEVICE_PASSWORD);
            if (pairedDevicePass != null) {
                Log.d(TAG, "onBLEServiceStateMsg: USED PAIR PASSWORD: "+pairedDevicePass);
                EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_LOGIN, pairedDevicePass));
            } else {
                // Password dialog
                final AlertDialog.Builder mBuilder = new AlertDialog.Builder(ScaleSelect.this);
                View mView = getLayoutInflater().inflate(R.layout.password_dialog, null);
                final EditText mPass = (EditText) mView.findViewById(R.id.etPassword);
                Button mBack = (Button) mView.findViewById(R.id.btnBack);
                Button mConfirm = (Button) mView.findViewById(R.id.btnConfirm);

                mBuilder.setView(mView);
                final AlertDialog dialog = mBuilder.create();
                dialog.show();

                mConfirm.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        if (!mPass.getText().toString().isEmpty()) {
                            EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_LOGIN, mPass.getText().toString()));
                            saveStringToStorage(PAIRED_DEVICE_PASSWORD, mPass.getText().toString());
                            dialog.dismiss();
                        } else {
                            Toast.makeText(getApplicationContext(), "Geslo prazno!", Toast.LENGTH_SHORT).show();
                        }
                    }
                });

                mBack.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_DISCONNECT));
                        dialog.dismiss();
                    }
                });
            }
        } else if (msg.state == BLE_VAR.BLE_SERVICE_WRONG_PASSWORD) {
            deleteFile(PAIRED_DEVICE_PASSWORD);
            Toast.makeText(getApplicationContext(), "Napačno geslo", Toast.LENGTH_SHORT).show();
        } else if (msg.state == BLE_VAR.BLE_SERVICE_LOGIN_SUCCESS) {
            Log.d(TAG, "onBLEServiceStateMsg: successful login");
            Intent i = new Intent(ScaleSelect.this, WeightActivity.class);
            startActivity(i);
        } else if(msg.state == BLE_VAR.BLE_SCAN_STOP) {
            swipeView.setRefreshing(false);
        } else if (msg.state == BLE_VAR.BLE_SERVICE_DISCONNECTED) {
            Toast.makeText(getApplicationContext(), "Povezava s tehtnico prekinjena!", Toast.LENGTH_SHORT).show();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        Log.d(TAG, "onActivityResult: "+requestCode);
        // wait for user to enable bluetooth
        if (requestCode == BLE_VAR.REQUEST_ENABLE_BT) {
            if (resultCode == RESULT_OK) {
                if (!hasLocationPermissions()) {
                    requestLocationPermission();
                }
            } else {
                Toast.makeText(this, "This application requeires bluetooth to be enabled.", Toast.LENGTH_SHORT).show();
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        // wait for user to enable location permissions
        if(requestCode == BLE_VAR.REQUEST_PERMISSION_LOCATION_FINE) {
            Log.d(TAG, "onRequestPermissionsResult: FINE_LOCATION: " + PackageManager.PERMISSION_GRANTED);
            if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {

            } else if (grantResults[0] == PackageManager.PERMISSION_DENIED) {
                Toast.makeText(getApplicationContext(), "Application needs Location Services to work.", Toast.LENGTH_SHORT).show();
            }
        }
    }

    private void requestBluetoothEnable() {
        Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
        startActivityForResult(enableBtIntent, BLE_VAR.REQUEST_ENABLE_BT);
        Log.d(TAG, "Requested user enables Bluetooth.");
    }

    private boolean hasLocationPermissions() {
        return checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }
    private void requestLocationPermission() {
        Log.d(TAG, "Requested user for location permissions");
        requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, BLE_VAR.REQUEST_PERMISSION_LOCATION_FINE);
    }

    /**
     * Starts BLEService and initializes BLEService if not initialized
     */
    private void startBLEService() {
        if (mBLEService == null) {
            mBLEService = new BLEService();
        }
        if(!mBLEService.isRunning) {
            startService(new Intent(this, mBLEService.getClass()));
        }
    }

    /**
     * Stops BLEService
     */
    private void stopBLEService() {
        if (mBLEService.isRunning) {
            stopService(new Intent(this, mBLEService.getClass()));
        }
    }

    private void saveStringToStorage(String fileName, String text) {
        FileOutputStream fos = null;
        try {
            fos = openFileOutput(fileName, MODE_PRIVATE);
            fos.write(text.getBytes());
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (fos != null) {
                try {
                    fos.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    private String loadStringFromStorage(String fileName) {
        FileInputStream fis = null;
        try {
            fis = openFileInput(fileName);
            InputStreamReader isr = new InputStreamReader(fis);
            BufferedReader br = new BufferedReader(isr);
            StringBuilder sb = new StringBuilder();
            String text;
            while ((text = br.readLine()) != null) {
                sb.append(text);
            }
            return sb.toString();
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (fis != null) {
                try {
                    fis.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
        return null;
    }
}