package com.ijs.saam;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

import com.ijs.saam.msg.BLEServiceMsg;
import com.ijs.saam.utils.BLE_VAR;

import org.greenrobot.eventbus.EventBus;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;

public class Settings extends AppCompatActivity {
    private String PAIRED_DEVICE_NAME = "paired_device_name";
    private String PAIRED_DEVICE_PASSWORD = "paired_device_password";
    private String DEVICE_IDENTIFIER = "device_identifier";

    private EditText etUser;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings);

        etUser = (EditText) findViewById(R.id.etUser);

        etUser.setText(loadStringFromStorage(DEVICE_IDENTIFIER));

        etUser.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) { }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                saveStringToStorage(DEVICE_IDENTIFIER, etUser.getText().toString());
            }

            @Override
            public void afterTextChanged(Editable s) { }
        });
    }

    public void onForgetScalePressed(View view) {
        // Password dialog
        final AlertDialog.Builder mBuilder = new AlertDialog.Builder(Settings.this);
        View mView = getLayoutInflater().inflate(R.layout.confirm_dialog, null);
        Button mBack = (Button) mView.findViewById(R.id.btnBack);
        Button mConfirm = (Button) mView.findViewById(R.id.btnConfirm);

        mBuilder.setView(mView);
        final AlertDialog dialog = mBuilder.create();
        dialog.show();

        mConfirm.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                deleteFile(PAIRED_DEVICE_NAME);
                deleteFile(PAIRED_DEVICE_PASSWORD);
                EventBus.getDefault().post(new BLEServiceMsg(BLE_VAR.BLE_DISCONNECT));
                dialog.dismiss();
                Settings.this.finish();
            }
        });

        mBack.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                dialog.dismiss();
            }
        });
    }

    public void onButtonConfirm(View view) {
        this.finish();
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