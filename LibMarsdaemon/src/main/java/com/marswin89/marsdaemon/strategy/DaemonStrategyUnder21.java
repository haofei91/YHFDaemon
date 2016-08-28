package com.marswin89.marsdaemon.strategy;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Log;

import com.marswin89.marsdaemon.DaemonConfigurations;
import com.marswin89.marsdaemon.IDaemonStrategy;
import com.marswin89.marsdaemon.nativ.NativeDaemonAPI20;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * the strategy in android API below 21.
 * 1. 实际上是
 *	   a. 主进程就是 service1在的服务,process1
 *	   b. 守护进程就是native的 daemon进程
 *	   辅助进程很快就死掉
 *
 * 2. 主进程管道监听守护进程：
 *    发现守护死亡，就调用java层的onDaemonDead，并自杀
 *    ---启动闹钟，开启辅助进程
 *    ----辅助进程开启后，onDaemonAssistantCreate开启主进程
 *    ----主进程杀了原来的守护进程，重建新的守护进程，并管道连接
 *    【为什么主进程发现守护进程死了，自己也要自杀重启一次，因为匿名管道连接父子进程，需要重新建立】
 *
 * 3. 子守护进程管道监听主进程：
 *    发现主进程死亡，使用native 命令开始辅助进程
 *    ----辅助进程开启后，onDaemonAssistantCreate开启主进程
 *    ----主进程杀了原来的守护进程，重建新的守护进程，并管道连接
 *    【为什么子进程发现主死亡，主启动后要重新杀了原守护进程，因为匿名管道连接父子进程，需要重新建立】
 * @author Mars
 *
 */
public class DaemonStrategyUnder21 implements IDaemonStrategy{
	private final String BINARY_DEST_DIR_NAME 	= "bin";
	private final String BINARY_FILE_NAME		= "daemon";
	
	private AlarmManager 			mAlarmManager;
	private PendingIntent			mPendingIntent;
	
	@Override
	public boolean onInitialization(Context context) {
		Log.i(TAG,"onInitialization，安装二进制文件");
		return installBinary(context);
	}

	@Override
	public void onPersistentCreate(final Context context, final DaemonConfigurations configs) {
		Log.i(TAG,"onPersistentCreate，常驻进程开启时调用");
		//搞一个闹钟去启动service2
		initAlarm(context, configs.DAEMON_ASSISTANT_CONFIG.SERVICE_NAME);
		Thread t = new Thread(){
			public void run() {
				File binaryFile = new File(context.getDir(BINARY_DEST_DIR_NAME, Context.MODE_PRIVATE), BINARY_FILE_NAME);
				//传递进去  “包名”和“辅助进程服务名称”和“二进制文件”
				//死的时候 启动第三辅助进程
				Log.i(TAG,"onPersistentCreate，进入JNI层：");
				new NativeDaemonAPI20(context).doDaemon(
						context.getPackageName(), 
						configs.DAEMON_ASSISTANT_CONFIG.SERVICE_NAME,
						binaryFile.getAbsolutePath());
			};
		};
		t.setPriority(Thread.MAX_PRIORITY);
		t.start();
		
		if(configs != null && configs.LISTENER != null){
			configs.LISTENER.onPersistentStart(context);
		}
	}

	//第三进程启动起来，就是负责把常驻进程拉起来，然后自杀掉。
	@Override
	public void onDaemonAssistantCreate(Context context, DaemonConfigurations configs) {
		Log.i(TAG,"第三辅助进程开启（只有在主或子死亡时才开启），调用onDaemonAssistantCreate，启动常驻进程，并自杀");
		Intent intent = new Intent();
		//service1
		ComponentName component = new ComponentName(context.getPackageName(), configs.PERSISTENT_CONFIG.SERVICE_NAME);
		intent.setComponent(component);
		context.startService(intent);
		if(configs != null && configs.LISTENER != null){
			configs.LISTENER.onWatchDaemonDaed();
		}
		android.os.Process.killProcess(android.os.Process.myPid());
	}
	

	//监听到子进程死了时候使用闹钟拉起第三个进程
	// 二进制文件监听到父进程死掉，直接用c代码发intent，见上面c代码
	@Override
	public void onDaemonDead() {
		Log.i(TAG,"onDaemonDead回调，并自杀"+android.os.Process.myPid());
		mAlarmManager.setRepeating(AlarmManager.ELAPSED_REALTIME, SystemClock.elapsedRealtime(), 100, mPendingIntent);
		android.os.Process.killProcess(android.os.Process.myPid());
	}

	//初始化一个alarm闹钟，用于开启  辅助的服务
	private void initAlarm(Context context, String serviceName){
		if(mAlarmManager == null){
            mAlarmManager = ((AlarmManager)context.getSystemService(Context.ALARM_SERVICE));
        }
        if(mPendingIntent == null){
            Intent intent = new Intent();
			//service2
    		ComponentName component = new ComponentName(context.getPackageName(), serviceName);
    		intent.setComponent(component);
            intent.setFlags(Intent.FLAG_EXCLUDE_STOPPED_PACKAGES);
            mPendingIntent = PendingIntent.getService(context, 0, intent, 0);
        }
		//注意设置闹钟前必须把以前设置的闹钟取消
        mAlarmManager.cancel(mPendingIntent);
	}
	

	//把二进制文件从assets目录中拷贝到跟目录下
	private boolean installBinary(Context context){
		String binaryDirName = null;
		String abi = Build.CPU_ABI;
		if (abi.startsWith("armeabi-v7a")) {
			binaryDirName = "armeabi-v7a";
		}else if(abi.startsWith("x86")) {
			binaryDirName = "x86";
		}else{
			binaryDirName = "armeabi";
		}
		return install(context, BINARY_DEST_DIR_NAME, binaryDirName, BINARY_FILE_NAME);
	}
	

	//destDirName=bin   assetsDirName=armeabi  filename=daemon
	private boolean install(Context context, String destDirName, String assetsDirName, String filename) {
		File file = new File(context.getDir(destDirName, Context.MODE_PRIVATE), filename);
		if (file.exists()) {
			return true;
		}
		try {
			copyAssets(context, (TextUtils.isEmpty(assetsDirName) ? "" : (assetsDirName + File.separator)) + filename, file, "700");
			return true;
		} catch (Exception e) {
			return false;
		}
	}

	//assetsFilename=armeabi-v7a/daemon  file=data/data/XXX/app_bin/daemon
	private void copyAssets(Context context, String assetsFilename, File file, String mode) throws IOException, InterruptedException {
		AssetManager manager = context.getAssets();
		final InputStream is = manager.open(assetsFilename);
		copyFile(file, is, mode);
	}
	
	private void copyFile(File file, InputStream is, String mode) throws IOException, InterruptedException {
		if(!file.getParentFile().exists()){
			file.getParentFile().mkdirs();
		}
		final String abspath = file.getAbsolutePath();
		final FileOutputStream out = new FileOutputStream(file);
		byte buf[] = new byte[1024];
		int len;
		while ((len = is.read(buf)) > 0) {
			out.write(buf, 0, len);
		}
		out.close();
		is.close();
		Runtime.getRuntime().exec("chmod " + mode + " " + abspath).waitFor();
	}
}
