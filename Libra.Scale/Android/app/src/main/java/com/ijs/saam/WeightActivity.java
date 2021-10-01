package com.ijs.saam;

/*
    Created by Andraz Simcic on 22/07/2020.
*/

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import com.ijs.saam.msg.ActivityMsg;
import com.ijs.saam.msg.BLEServiceMsg;
import com.ijs.saam.msg.BLEServiceStateMsg;
import com.ijs.saam.utils.ACTIVITY_VAR;
import com.ijs.saam.utils.BLE_VAR;
import com.ijs.saam.utils.ScaleUUID;

import org.greenrobot.eventbus.EventBus;
import org.greenrobot.eventbus.Subscribe;
import org.greenrobot.eventbus.ThreadMode;

public class WeightActivity extends AppCompatActivity {
    private static final String TAG = "WeightActivity";

    private int BATTERY_REFRESH_TIME = 30000;
    private String WEIGHT_VAR = "weight_var";
    private TextView tvWeight;
    private TextView tvBattery;
    private ImageView ivBattery;
    private Handler batteryHandler;
    private boolean batteryHandlerActive = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_weight);

        tvWeight = (TextView)findViewById(R.id.tvWeight);
        tvBattery = (TextView) findViewById(R.id.tvBattery);
        ivBattery = (ImageView) findViewById(R.id.ivBattery);

        ivBattery.setImageLevel(1000);

        // create a handler that checks scale battery level every 30 sec
        //startBatteryHandler();

        EventBus.getDefault().register(this);
    }

    @Override
    protected void onStart() {
        super.onStart();
        // READ status of scale DEBUGGING
        EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_CHARA_READ, ScaleUUID.SERVICE_CONFIG, ScaleUUID.CHARA_GET_CUR_SETTINGS));
        //EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_CHARA_READ, ScaleUUID.SERVICE_BATTERY, ScaleUUID.CHARA_BATTERY_CUSTOM));

        EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_CHARA_READ, ScaleUUID.SERVICE_BATTERY, ScaleUUID.CHARA_BATTERY_CUSTOM));
        startBatteryHandler();
        EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_NOTIFICATION, ScaleUUID.SERVICE_WEIGHT, ScaleUUID.CHARA_WEIGHT, true, true));
    }

    @Override
    protected void onStop() {
        super.onStop();
        EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_NOTIFICATION, ScaleUUID.SERVICE_WEIGHT, ScaleUUID.CHARA_WEIGHT, false, false));
        stopBatteryHandler();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy: destroying WeightActivity");
        EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_NOTIFICATION, ScaleUUID.SERVICE_WEIGHT, ScaleUUID.CHARA_WEIGHT, false, false));
        stopBatteryHandler();
        backClicked(null);
        EventBus.getDefault().unregister(this);
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    public void onBLEServiceStateMsg(BLEServiceStateMsg msg) {
        Log.d(TAG, "onBLEServiceStateMsg: STATE: "+msg.state);
        if (msg.state == BLE_VAR.BLE_SERVICE_WEIGHT_NOTIFICATION) {
            String cleanWeightText = "";
            cleanWeightText = Integer.toString(Integer.parseInt(msg.s1.replaceAll(" |g", "").replaceAll(" |G", ""))) + " g";
            Log.d(TAG, "onBLEServiceStateMsg: WEIGHT = " + cleanWeightText);
            tvWeight.setText(cleanWeightText);
        } else if (msg.state == BLE_VAR.BLE_SERVICE_BATTERY_READ) {
            int percent = Integer.parseInt(msg.s1.substring(0, 3));
            String batteryPercent = Integer.toString(percent);
            tvBattery.setText(batteryPercent+"%");
            ivBattery.setImageLevel(percent);
        } else if (msg.state == BLE_VAR.BLE_SERVICE_DISCONNECTED) {
            Log.d(TAG, "onBLEServiceStateMsg: DISCONNECT DETECTED!");
            backClicked(null);
        } else if (msg.state == BLE_VAR.BLE_SERVICE_SETTINGS_READ) {
            Log.d(TAG, "onBLEServiceStateMsg: SETTINGS READ! \n\t"+msg.state+"\n\t"+msg.s1);
        }
    }

    /*@Subscribe(threadMode = ThreadMode.MAIN)
    public void onActivityMsg(ActivityMsg msg) {
        if (msg.activityToClose == ACTIVITY_VAR.ACTIVITY_WEIGHT) {
            finish();
        }
    }*/

    /**
     * close this activity and stop weight logging,
     * disconnect from the scale
     */
    public void backClicked(View view) {
        EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_NOTIFICATION, ScaleUUID.SERVICE_WEIGHT, ScaleUUID.CHARA_WEIGHT, false, false));
        EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_DISCONNECT));
        EventBus.getDefault().unregister(this);
        finish();
    }

    public void tareClicked(View view) {
        EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_SET_TARE));
    }

    public void confirmClick(View view) {
        String weight = tvWeight.getText().toString();
        Intent i = new Intent(WeightActivity.this, TakePhoto.class);
        i.putExtra(WEIGHT_VAR, weight);
        startActivity(i);
    }

    /**
     * Starts battery handler, that checks for battery info every @BATTERY_REFRESH_TIME
     */
    private void startBatteryHandler() {
        batteryHandler = new Handler();
        batteryHandlerActive = true;
        batteryHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (batteryHandlerActive) {
                    Log.d(TAG, "run: read battery");
                    EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_CHARA_READ, ScaleUUID.SERVICE_BATTERY, ScaleUUID.CHARA_BATTERY_CUSTOM));
                    batteryHandler.postDelayed(this, BATTERY_REFRESH_TIME);
                }
            }
        }, BATTERY_REFRESH_TIME);
    }

    private void stopBatteryHandler() {
        if (batteryHandlerActive) {
            batteryHandlerActive = false;
            batteryHandler.removeCallbacksAndMessages(null);
            batteryHandler = null;
        }
    }

    public void onInfoClick(View view) {
        Intent i = new Intent(getApplicationContext(), Settings.class);
        startActivity(i);
    }
}