package com.marswin89.marsdaemon;

import android.content.Context;
import android.os.Build;
import android.util.Log;

import com.marswin89.marsdaemon.strategy.DaemonStrategy21;
import com.marswin89.marsdaemon.strategy.DaemonStrategyUnder21;

/**
 * define strategy method
 * 
 * @author Mars
 *
 */
public interface IDaemonStrategy {

	public static String TAG="YHF-IDaemonStrategy";
	/**
	 * Initialization some files or other when 1st time 
	 * 
	 * @param context
	 * @return
	 */
	boolean onInitialization(Context context);

	/**
	 * when Persistent process create
	 * 
	 * @param context
	 * @param configs
	 */
	void onPersistentCreate(Context context, DaemonConfigurations configs);

	/**
	 * when DaemonAssistant process create
	 * @param context
	 * @param configs
	 */
	void onDaemonAssistantCreate(Context context, DaemonConfigurations configs);

	/**
	 * when watches the process dead which it watched
	 */
	void onDaemonDead();

	
	
	/**
	 * all about strategy on different device here
	 * 
	 * @author Mars
	 *
	 */
	public static class Fetcher {

		private static IDaemonStrategy mDaemonStrategy;

		/**
		 * fetch the strategy for this device
		 * 
		 * @return the daemon strategy for this device
		 */
		static IDaemonStrategy fetchStrategy() {
			if (mDaemonStrategy != null) {
				return mDaemonStrategy;
			}
			int sdk = Build.VERSION.SDK_INT;
			Log.i(TAG,"fetchStrategy，SDK_INT: "+sdk);
			switch (sdk) {

				case 22:
					mDaemonStrategy = new DaemonStrategy21();
					break;
				
				case 21:
					if("MX4 Pro".equalsIgnoreCase(Build.MODEL)){
						mDaemonStrategy = new DaemonStrategyUnder21();
					}else{
						mDaemonStrategy = new DaemonStrategy21();
					}
					break;
				
				default:
					 if(Build.MODEL != null && Build.MODEL.toLowerCase().startsWith("a31")){
						mDaemonStrategy = new DaemonStrategy21();
					}else{
						mDaemonStrategy = new DaemonStrategyUnder21();
					}
					break;
			}
			return mDaemonStrategy;
		}
	}
}
