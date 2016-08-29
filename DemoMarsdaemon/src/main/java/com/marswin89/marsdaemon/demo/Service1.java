package com.marswin89.marsdaemon.demo;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

/**
 * This Service is Persistent Service. Do some what you want to do here.<br/>
 *
 * Created by Mars on 12/24/15.
 */
public class Service1 extends Service{

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i("YHF-Service1","Service1 onCreate ");
        //TODO do some thing what you want..
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
