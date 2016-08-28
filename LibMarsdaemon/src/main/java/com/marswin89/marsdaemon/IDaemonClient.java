package com.marswin89.marsdaemon;

import android.content.Context;

/**
 * 
 * @author Mars
 *
 */
public interface IDaemonClient {
	public static String TAG="YHF-IDaemonClient";
	/**
	 * override this method by {@link android.app.Application}</br></br>
	 * ****************************************************************</br>
	 * <b>DO super.attchBaseContext() first !</b></br>
	 * ****************************************************************</br>
	 * 
	 * @param base
	 */
	void onAttachBaseContext(Context base);
}
