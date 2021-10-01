package com.ijs.saam;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.os.Handler;

import com.ijs.saam.msg.ActivityMsg;
import com.ijs.saam.utils.ACTIVITY_VAR;

import org.greenrobot.eventbus.EventBus;

public class End extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_end);

        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                EventBus.getDefault().post(new ActivityMsg(ACTIVITY_VAR.ACTIVITY_PHOTO));
                EventBus.getDefault().post(new ActivityMsg(ACTIVITY_VAR.ACTIVITY_CONTENT));
                EventBus.getDefault().post(new ActivityMsg(ACTIVITY_VAR.ACTIVITY_WEIGHT));
            }
        }, 2000);
    }

    @Override
    protected void onStop() {
        super.onStop();
        finish();
    }
}