package com.ijs.saam;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;

import java.util.Locale;

public class SplashScreen extends AppCompatActivity {
    private static int SPLASH_SCREEN_TIMER = 1000;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_splash_screen);
        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                setLocale("en");
                Intent i = new Intent(SplashScreen.this, ScaleSelect.class);
                //Intent i = new Intent(SplashScreen.this, MealContents.class);
                startActivity(i);
                finish();
            }
        }, SPLASH_SCREEN_TIMER);
    }

    private void setLocale(String language) {
        Locale locale = new Locale(language);
        locale.setDefault(locale);
        Configuration config = new Configuration();
        config.setLocale(locale);
        getBaseContext().getResources().updateConfiguration(config, getBaseContext().getResources().getDisplayMetrics());
        SharedPreferences.Editor editor = getSharedPreferences("Settings", MODE_PRIVATE).edit();
        editor.putString("My_Lang", language);
        editor.apply();
    }
}