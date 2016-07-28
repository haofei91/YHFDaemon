package com.marswin89.marsdaemon.strategy;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.SystemClock;
import android.util.Log;

import com.marswin89.marsdaemon.DaemonConfigurations;
import com.marswin89.marsdaemon.IDaemonStrategy;
import com.marswin89.marsdaemon.nativ.NativeDaemonAPI21;

import java.io.File;
import java.io.IOException;
/**
 * the strategy in android API 21.
 * 1. 实际上是
 * 	  a. 主进程就是 service1在的服务  process1
 * 	  b. 守护进程就 辅助进程   process2
 * 	  没有native层的damon进程了，  主进程和守护进程跑相同的逻辑，锁上文件，然后等待对方的文件锁
 * @author Mars
 *
 */
public class DaemonStrategy21 implements IDaemonStrategy{
	private final static String INDICATOR_DIR_NAME 					= "indicators";
	private final static String INDICATOR_PERSISTENT_FILENAME 		= "indicator_a";
	private final static String INDICATOR_DAEMON_ASSISTANT_FILENAME = "indicator_b";
	private final static String OBSERVER_PERSISTENT_FILENAME		= "observer_a";
	private final static String OBSERVER_DAEMON_ASSISTANT_FILENAME	= "observer_b";
	
	private AlarmManager			mAlarmManager;
	private PendingIntent 			mPendingIntent;
	private DaemonConfigurations 	mConfigs;

	@Override
	public boolean onInitialization(Context context) {
		Log.i(TAG,"onInitialization，生成锁文件");
		return initIndicators(context);
	}

	@Override
	public void onPersistentCreate(final Context context, DaemonConfigurations configs) {
		Log.i(TAG,"onPersistentCreate，常驻进程开启时调用");

		//注意这里 和21以下不同，此处直接开启  第二个进程服务
		Intent intent = new Intent();
		ComponentName componentName = new ComponentName(context.getPackageName(), configs.DAEMON_ASSISTANT_CONFIG.SERVICE_NAME);
		intent.setComponent(componentName);
		Log.i(TAG,"onPersistentCreate，双进程模式，开启辅助服务");
		context.startService(intent);

		//启动常驻进程的闹钟  service1
		initAlarm(context, configs.PERSISTENT_CONFIG.SERVICE_NAME);
		
		Thread t = new Thread(){
			@Override
			public void run() {
				Log.i(TAG,"onPersistentCreate，进入JNI层：");
				File indicatorDir = context.getDir(INDICATOR_DIR_NAME, Context.MODE_PRIVATE);
				new NativeDaemonAPI21(context).doDaemon(
						new File(indicatorDir, INDICATOR_PERSISTENT_FILENAME).getAbsolutePath(), 
						new File(indicatorDir, INDICATOR_DAEMON_ASSISTANT_FILENAME).getAbsolutePath(), 
						new File(indicatorDir, OBSERVER_PERSISTENT_FILENAME).getAbsolutePath(),
						new File(indicatorDir, OBSERVER_DAEMON_ASSISTANT_FILENAME).getAbsolutePath());
			}
		};
		t.setPriority(Thread.MAX_PRIORITY);
		t.start();
		
		if(configs != null && configs.LISTENER != null){
			this.mConfigs = configs;
			configs.LISTENER.onPersistentStart(context);
		}
	}

	//第三进程启动起来，就是负责把常驻进程拉起来
	@Override
	public void onDaemonAssistantCreate(final Context context, DaemonConfigurations configs) {
		Log.i(TAG,"第三辅助进程开启，调用onDaemonAssistantCreate，启动常驻进程，（不再自杀）");
		Intent intent = new Intent();
		ComponentName componentName = new ComponentName(context.getPackageName(), configs.PERSISTENT_CONFIG.SERVICE_NAME);
		intent.setComponent(componentName);
		Log.i(TAG,"onPersistentCreate，双进程模式，开启常驻服务");
		context.startService(intent);

		//启动常驻进程的闹钟  service1
		initAlarm(context, configs.PERSISTENT_CONFIG.SERVICE_NAME);
		
		Thread t = new Thread(){
			public void run() {
				Log.i(TAG,"onDaemonAssistantCreate，进入JNI层：");
				File indicatorDir = context.getDir(INDICATOR_DIR_NAME, Context.MODE_PRIVATE);
				new NativeDaemonAPI21(context).doDaemon(
						new File(indicatorDir, INDICATOR_DAEMON_ASSISTANT_FILENAME).getAbsolutePath(), 
						new File(indicatorDir, INDICATOR_PERSISTENT_FILENAME).getAbsolutePath(), 
						new File(indicatorDir, OBSERVER_DAEMON_ASSISTANT_FILENAME).getAbsolutePath(),
						new File(indicatorDir, OBSERVER_PERSISTENT_FILENAME).getAbsolutePath());
			};
		};
		t.setPriority(Thread.MAX_PRIORITY);
		t.start();
		
		if(configs != null && configs.LISTENER != null){
			this.mConfigs = configs;
			configs.LISTENER.onDaemonAssistantStart(context);
		}
	}

	//监听到子进程死了时候使用闹钟拉起第三个进程
	// 二进制文件监听到父进程死掉，直接用c代码发intent，见上面c代码
	@Override
	public void onDaemonDead() {
		Log.i(TAG,"onDaemonDead回调，并自杀"+android.os.Process.myPid());
		mAlarmManager.setRepeating(AlarmManager.ELAPSED_REALTIME, SystemClock.elapsedRealtime(), 100, mPendingIntent);
		
		if(mConfigs != null && mConfigs.LISTENER != null){
			mConfigs.LISTENER.onWatchDaemonDaed();
		}
		android.os.Process.killProcess(android.os.Process.myPid());
	}


	//初始化一个alarm闹钟，用于开启  辅助的服务
	private void initAlarm(Context context, String serviceName){
		if(mAlarmManager == null){
            mAlarmManager = ((AlarmManager)context.getSystemService(Context.ALARM_SERVICE));
        }
        if(mPendingIntent == null){
            Intent intent = new Intent();
			ComponentName component = new ComponentName(context.getPackageName(), serviceName);
			intent.setComponent(component);
            intent.setFlags(Intent.FLAG_EXCLUDE_STOPPED_PACKAGES);
            mPendingIntent = PendingIntent.getService(context, 0, intent, 0);
        }
        mAlarmManager.cancel(mPendingIntent);
	}
	
	
	private boolean initIndicators(Context context){
		File dirFile = context.getDir(INDICATOR_DIR_NAME, Context.MODE_PRIVATE);
		if(!dirFile.exists()){
			dirFile.mkdirs();
		}
		try {
			createNewFile(dirFile, INDICATOR_PERSISTENT_FILENAME);
			createNewFile(dirFile, INDICATOR_DAEMON_ASSISTANT_FILENAME);
			return true;
		} catch (IOException e) {
			e.printStackTrace();
			return false;
		}
	}
	
	
	private void createNewFile(File dirFile, String fileName) throws IOException{
		File file = new File(dirFile, fileName);
		if(!file.exists()){
			file.createNewFile();
		}
	}

}
