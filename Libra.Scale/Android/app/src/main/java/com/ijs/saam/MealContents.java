package com.ijs.saam;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.media.session.MediaSession;
import android.os.Bundle;
import android.util.Base64;
import android.util.Log;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.Toast;

import com.ijs.saam.msg.ActivityMsg;
import com.ijs.saam.msg.CustomMsg;
import com.ijs.saam.utils.ACTIVITY_VAR;
import com.ijs.saam.utils.SocketFactory;

import org.eclipse.paho.android.service.MqttAndroidClient;
import org.eclipse.paho.client.mqttv3.IMqttActionListener;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.IMqttToken;
import org.eclipse.paho.client.mqttv3.MqttCallback;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.greenrobot.eventbus.EventBus;
import org.greenrobot.eventbus.Subscribe;
import org.greenrobot.eventbus.ThreadMode;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.security.KeyManagementException;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.UnrecoverableKeyException;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Date;
import java.util.Properties;
import java.util.UUID;

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;

import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;


public class MealContents extends AppCompatActivity {
    private static final String TAG = "MealContents";
    private static final String TAGG = "MQTT";
    private String WEIGHT_VAR = "weight_var";
    private String PICTURE_VAR = "picture_var";
    private String LOCATION_ID = "SIE7";
    private String SOURCE_ID = "sens_food_scale";
    private String DEVICE_IDENTIFIER = "device_identifier";

    private CustomSelectElement cseDairy;
    private CustomSelectElement cseEggs;
    private CustomSelectElement cseMeat;
    private CustomSelectElement cseFish;
    private CustomSelectElement cseBeans;
    private CustomSelectElement cseNone;
    private LinearLayout llContents;
    private ArrayList<CustomSelectElement> allOptions = new ArrayList<>();
    private Context context;

    //public static final String BROKER = "ssl://mqtt.dev-saam-platform.eu:8883";
    public static final String BROKER = "ssl://gregor.ijs.si:8883";
    public static final String TOPIC = "#";
    //public static final String USERNAME = "DeviceUser";
    public static final String USERNAME = "testni";
    //public static final String PASSWORD = "b9BpukeK";
    public static final String PASSWORD = "smoprimajdi";
    public MqttAndroidClient CLIENT;
    public MqttConnectOptions MQTT_CONNECTION_OPTIONS;
    private String AuthBearerToken = "";

    @Subscribe(threadMode = ThreadMode.MAIN)
    public void onActivityMsg(ActivityMsg msg) {
        if (msg.activityToClose == ACTIVITY_VAR.ACTIVITY_CONTENT) {
            finish();
        }
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    public void onCustomMsg(CustomMsg msg) {
        Log.d(TAG, "onCustomMsg: CUSTOM MSG "+ msg.data1);
        if(msg.data1 == ACTIVITY_VAR.REFRESH_CONTENT) {
            boolean oneSelected = false;
            for (CustomSelectElement cse : allOptions) {
                if (cse.isSelected()) {
                    oneSelected = true;
                }
            }
            if (oneSelected) {
                findViewById(R.id.btnConfirm).setEnabled(true);
            } else {
                findViewById(R.id.btnConfirm).setEnabled(false);
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_meal_contents);
        context = getApplicationContext();

        EventBus.getDefault().register(this);

        /*
        // PAHO SSL CONNECTION
        setupMQTT(context.getResources().openRawResource(R.raw.saam_ca));
        //MqttSetup(context.getResources().openRawResource(R.raw.saam_ca));
        publish("test", "Testni string");

        CLIENT.setCallback(new MqttCallback() {
            @Override
            public void connectionLost(Throwable cause) {

            }

            //background notification
            @Override
            public void messageArrived(String topic, MqttMessage message) throws Exception {
                Log.d(TAGG, "message:" + message.toString());
            }

            @Override
            public void deliveryComplete(IMqttDeliveryToken token) {

            }
        });
         */


        cseDairy = (CustomSelectElement) findViewById(R.id.cseDairy);
        cseEggs = (CustomSelectElement) findViewById(R.id.cseEggs);
        cseMeat = (CustomSelectElement) findViewById(R.id.cseMeat);
        cseFish = (CustomSelectElement) findViewById(R.id.cseFish);
        cseBeans = (CustomSelectElement) findViewById(R.id.cseBeans);
        cseNone = (CustomSelectElement) findViewById(R.id.cseNone);
        llContents = (LinearLayout) findViewById(R.id.llContents);

        allOptions.add(cseDairy);
        allOptions.add(cseEggs);
        allOptions.add(cseMeat);
        allOptions.add(cseFish);
        allOptions.add(cseBeans);
        allOptions.add(cseNone);

        Log.d(TAG, "onCreate: WEIGHT: "+getIntent().getStringExtra(WEIGHT_VAR));
        try {
            Log.d(TAG, "onCreate: BITMAP: "+ BitmapFactory.decodeStream(getApplicationContext().openFileInput(PICTURE_VAR)));
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        };

        getAuthenticationToken();
    }

    private void getAuthenticationToken() {
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                OkHttpClient client = new OkHttpClient().newBuilder()
                        .build();
                MediaType mediaType = MediaType.parse("application/x-www-form-urlencoded");
                RequestBody body = RequestBody.create(mediaType, "scope=sensor-api&client_id=saam-dietary-app&grant_type=client_credentials&client_secret=11bebc23082cec9e8a97cf6a1e2437a1cc");
                Request request = new Request.Builder()
                        .url("https://auth.dev-saam-platform.eu/connect/token")
                        .method("POST", body)
                        .addHeader("Content-Type", "application/x-www-form-urlencoded")
                        .build();
                try {
                    Response response = client.newCall(request).execute();
                    String responseJSONString = response.body().string();
                    if(response.isSuccessful()) {
                        JSONObject responseJSON = new JSONObject(responseJSONString);
                        Log.d(TAG, "run: " + responseJSON.get("access_token").toString());
                        AuthBearerToken = responseJSON.get("access_token").toString();
                    }
                } catch (IOException | JSONException e) {
                    e.printStackTrace();
                }
            }
        });
        thread.start();
    }

    private void setupMQTT(InputStream CaCert) {
        String clientID="";
        clientID = loadStringFromStorage(DEVICE_IDENTIFIER);
        if (clientID == null) {
            clientID = UUID.randomUUID().toString();
        }
        try {
            CLIENT = new MqttAndroidClient(getBaseContext(), BROKER, clientID);

            MQTT_CONNECTION_OPTIONS = new MqttConnectOptions();
            MQTT_CONNECTION_OPTIONS.setUserName(USERNAME);
            MQTT_CONNECTION_OPTIONS.setPassword(PASSWORD.toCharArray());
            MQTT_CONNECTION_OPTIONS.setConnectionTimeout(60);
            MQTT_CONNECTION_OPTIONS.setKeepAliveInterval(60);
            MQTT_CONNECTION_OPTIONS.setMqttVersion(MqttConnectOptions.MQTT_VERSION_3_1);

            SocketFactory.SocketFactoryOptions socketFactoryOptions = new SocketFactory.SocketFactoryOptions();

            CertificateFactory cf = CertificateFactory.getInstance("X.509");
            Certificate ca;
            try {
                ca = cf.generateCertificate(CaCert);
                Log.d(TAGG, "setupMQTT: ca="+((X509Certificate) ca).getSubjectDN());

                //socketFactoryOptions.withCaInputStream(CaCert);
                //MQTT_CONNECTION_OPTIONS.setSocketFactory(new SocketFactory(socketFactoryOptions));
            } finally {
                CaCert.close();
            }
            // Create a KeyStore containing our trusted CAs
            String keyStoreType = KeyStore.getDefaultType();
            KeyStore keyStore = KeyStore.getInstance(keyStoreType);
            keyStore.load(null,null);
            keyStore.setCertificateEntry("ca", ca);

            // Create a TrustManager that trusts the CAs in our KeyStore
            String tmfAlgorithm = TrustManagerFactory.getDefaultAlgorithm();
            TrustManagerFactory tmf = TrustManagerFactory.getInstance(tmfAlgorithm);
            tmf.init(keyStore);

            /*
            KeyStore trustSt = KeyStore.getInstance("BKS");
            TrustManagerFactory trustManagerFactory = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
            trustSt.load(CaCert, null);
            trustManagerFactory.init(trustSt);
             */


            // Create an SSLContext that uses our TrustManager
            SSLContext sslContext = SSLContext.getInstance("TLS");
            sslContext.init(null, tmf.getTrustManagers(), null);
            //sslContext.init(null, trustManagerFactory.getTrustManagers(), null);

            // Tell the MQTT which socket factory to use
            MQTT_CONNECTION_OPTIONS.setSocketFactory(sslContext.getSocketFactory());
            /*Properties sslClientProps = new Properties();
            sslClientProps.setProperty("com.ibm.ssl.protocol", "TLSv1.2");
            MQTT_CONNECTION_OPTIONS.setSSLProperties(sslClientProps);*/

            /* catch (IOException | NoSuchAlgorithmException | KeyStoreException | CertificateException | KeyManagementException | UnrecoverableKeyException e) {
                e.printStackTrace();
            }*/
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    void publish(String topic, String msg) {

        //0 is the Qos
        MQTT_CONNECTION_OPTIONS.setWill(topic, msg.getBytes(), 0, false);
        try {
            IMqttToken token = CLIENT.connect(MQTT_CONNECTION_OPTIONS);
            token.setActionCallback(new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    Log.d(TAGG, "send done" + asyncActionToken.toString());
                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    Log.d(TAGG, "publish error " + asyncActionToken.toString());
                    Log.e(TAGG, "onFailure: "+exception.getMessage());
                }
            });
        } catch (Exception e) {
            e.printStackTrace();
        }

    }


    @Override
    protected void onDestroy() {
        super.onDestroy();
        EventBus.getDefault().unregister(this);
        //disconnect();
    }


    public void onConfirm(View view) {
        //publish("test", "to je test");
        ArrayList<String> selectedFood = new ArrayList<>();
        for (CustomSelectElement i : allOptions) {
            if (i.isSelected()) {
                selectedFood.add(i.getName());
            }
        }
        Log.d(TAG, "onConfirm: SELECTED: " + selectedFood.toString());
        /*
        JSONObject sendData = new JSONObject();
        JSONObject measurements = new JSONObject();
        try {
            measurements.put("weight", getIntent().getStringExtra(WEIGHT_VAR));
            measurements.put("pictureFormat", "JPEG");
            measurements.put("pictureBase64", load(PICTURE_VAR));
            measurements.put("contains", selectedFood);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        try {
            sendData.put("timestamp", new Date().getTime());
            sendData.put("timestep", 0);
            sendData.put("measurements", measurements);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        Log.d(TAGG, "onConfirm: " + sendData.toString());

        String topic = "saam/data"+"/"+LOCATION_ID+"/"+SOURCE_ID;
        */
        POSTData(selectedFood);
        //Intent i = new Intent(getApplicationContext(), End.class);
        //startActivity(i);
    }

    private void POSTData(ArrayList<String> selectedFood) {
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                OkHttpClient client = new OkHttpClient().newBuilder()
                        .build();
                MediaType mediaType = MediaType.parse("application/json");


                String selectedFoodString = "[";
                for(int i=0; i<selectedFood.size(); i++) {
                    if (i>0) {
                        selectedFoodString += ",";
                    }
                    selectedFoodString += "\"";
                    selectedFoodString += selectedFood.get(i).toString();
                    selectedFoodString += "\"";
                }
                selectedFoodString += "]";
                Log.d(TAG, "run: STRING: " + selectedFoodString);

                RequestBody body = RequestBody.create(mediaType, "{\r\n" +
                        "\"id\": \"" + loadStringFromStorage(DEVICE_IDENTIFIER) + "\",\r\n" +
                        "\"locationId\": \"" + LOCATION_ID + "\",\r\n" +
                        "\"sourceId\": \"" + SOURCE_ID + "\",\r\n" +
                        "\"data\": {\r\n" +
                            "\"timestamp\": " + new Date().getTime() + ",\r\n" +
                            "\"timestep\": -1,\r\n" +
                            "\"measurements\": {\r\n" +
                                "\"weight\": " + getIntent().getStringExtra(WEIGHT_VAR).split(" ")[0] + ",\r\n" +
                                "\"image_png\": \"" + load(PICTURE_VAR) + "\",\r\n" +
                                "\"content\": "+
                                    selectedFoodString +
                                "\r\n" +
                            "}\r\n" +
                        "}\r\n}");

                //RequestBody body = RequestBody.create(mediaType, "{\r\n    \"id\": \"string\",\r\n    \"locationId\": \"string\",\r\n    \"sourceId\": \"string\",\r\n    \"data\": {\r\n        \"timestamp\": 0,\r\n        \"timestep\": 0,\r\n        \"measurements\": {\r\n            \"weight\": 320,\r\n            \"image_png\": \"j389rh2389r9023hjdf323df28d280\",\r\n            \"content\": [\r\n                \"a\",\r\n                \"b\"\r\n            ]\r\n        }\r\n    }\r\n}");
                Request request = new Request.Builder()
                        .url("https://sensors.dev-saam-platform.eu/Dietary/"+LOCATION_ID)
                        .method("POST", body)
                        .addHeader("Authorization", "Bearer "+AuthBearerToken)
                        .addHeader("Content-Type", "application/json")
                        .build();
                try {
                    Log.d(TAG, "run: SENDING...");
                    Response response = client.newCall(request).execute();
                    if (response.isSuccessful()) {
                        JSONObject responseJSON = new JSONObject(response.body().string());
                        Log.d(TAG, "POSTData: " + response.code());
                        Log.d(TAG, "POSTData: " + response.message());
                        Log.d(TAG, "POSTData: " + responseJSON.toString());
                        Intent i = new Intent(getApplicationContext(), End.class);
                        startActivity(i);
                    } else {
                        Log.d(TAG, "run: UNSUCCESSFULL");
                        Log.d(TAG, "run: " + response.message());
                        Log.d(TAG, "run: " + response.code());
                        Log.d(TAG, "run: " + response.body().toString());
                    }
                } catch (IOException | JSONException e) {
                    e.printStackTrace();
                    Log.e(TAG, "run: ", e);
                }
            }
        });
        thread.start();
    }

    public void onInfoClick(View view) {
        Intent i = new Intent(getApplicationContext(), Settings.class);
        startActivity(i);
    }

    public void onBackClick(View view) {
        finish();
    }


    /**
     * Loads picture resource from internal storage as string.
     * @param fileName name of the file.
     * @return String base64 format of the picture taken.
     */
    public String load(String fileName) {
        try {
            FileInputStream fis = getApplicationContext().openFileInput(fileName);
            InputStreamReader isr = new InputStreamReader(fis, "UTF-8");
            BufferedReader bufferedReader = new BufferedReader(isr);
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = bufferedReader.readLine()) != null) {
                sb.append(line).append("\n");
            }
            return sb.toString();
        } catch (FileNotFoundException e) {
            return "";
        } catch (UnsupportedEncodingException e) {
            return "";
        } catch (IOException e) {
            return "";
        }
    }

    private String bitmapToBase64(Bitmap bitmap) {
        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.JPEG, 100, byteArrayOutputStream);
        byte[] byteArray = byteArrayOutputStream .toByteArray();
        return Base64.encodeToString(byteArray, Base64.DEFAULT);
    }

    public static Bitmap RotateBitmap(Bitmap source, float angle) {
        Matrix matrix = new Matrix();
        matrix.postRotate(angle);
        return Bitmap.createBitmap(source, 0, 0, source.getWidth(), source.getHeight(), matrix, true);
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