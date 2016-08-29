package com.marswin89.marsdaemon.demo;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

/**
 * DO NOT do anything in this Receiver!<br/>
 *
 * Created by Mars on 12/24/15.
 */
public class Receiver1 extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        Log.i("YHF-Receiver1","Receiver1 onReceive ");
    }
}
