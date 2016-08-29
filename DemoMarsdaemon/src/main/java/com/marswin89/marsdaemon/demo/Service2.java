package com.marswin89.marsdaemon.demo;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

/**
 * DO NOT do anything in this Service!<br/>
 *
 * Created by Mars on 12/24/15.
 */
public class Service2 extends Service{

    @Override
    public IBinder onBind(Intent intent) {
        Log.i("YHF-Service1","Service1 onCreate ");
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return Service.START_NOT_STICKY;
    }
}
