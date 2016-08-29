/*
 * File        : daemon_api21.c
 * Author      : Mars Kwok
 * Date        : Jul. 21, 2015
 * Description : This is used to watch process dead over api 21
 *
 * proces1         主         process2            java层  避免了同组问题
 *
 * 原始代码： android启动两个进程，proces1和process2相互监听
 * 但是不解决“同包名”“task”问题
 * 在一些机型上，如魅族，根本观察不到进程死亡，原因就是java层的“瞬杀”
 * Copyright (C) Mars Kwok<Marswin89@gmail.com>
 *
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/inotify.h>
//用于修改进程名
#include <sys/prctl.h>


#include "com_marswin89_marsdaemon_nativ_NativeDaemonAPI21.h"
#include "log.h"
#include "constant.h"

#define TAG ("YHF-Client21")

void waitfor_self_observer(char* observer_file_path){
	int lockFileDescriptor = open(observer_file_path, O_RDONLY);
	if (lockFileDescriptor == -1){
		LOGI(TAG,"Watched >>>>OBSERVER<<<< has been ready before watching...");
		return ;
	}

	void *p_buf = malloc(sizeof(struct inotify_event));
	if (p_buf == NULL){
		LOGI(TAG,"malloc failed !!!");
		return;
	}
	int maskStrLength = 7 + 10 + 1;
	char *p_maskStr = malloc(maskStrLength);
	if (p_maskStr == NULL){
		free(p_buf);
		LOGI(TAG,"malloc failed !!!");
		return;
	}
	int fileDescriptor = inotify_init();
	if (fileDescriptor < 0){
		free(p_buf);
		free(p_maskStr);
		LOGI(TAG,"inotify_init failed !!!");
		return;
	}

	int watchDescriptor = inotify_add_watch(fileDescriptor, observer_file_path, IN_ALL_EVENTS);
	if (watchDescriptor < 0){
		free(p_buf);
		free(p_maskStr);
		LOGI(TAG,"inotify_add_watch failed !!!");
		return;
	}


	while(1){
		size_t readBytes = read(fileDescriptor, p_buf, sizeof(struct inotify_event));
		if (4 == ((struct inotify_event *) p_buf)->mask){
			LOGI(TAG,"Watched >>>>OBSERVER<<<< has been ready...");
			free(p_maskStr);
			free(p_buf);
			return;
		}
	}
}

void notify_daemon_observer(unsigned char is_persistent, char* observer_file_path){
	if(!is_persistent){
		int lockFileDescriptor = open(observer_file_path, O_RDONLY);
		while(lockFileDescriptor == -1){
			lockFileDescriptor = open(observer_file_path, O_RDONLY);
		}
	}
	remove(observer_file_path);
}

notify_and_waitfor(char *observer_self_path, char *observer_daemon_path){
    LOGI(TAG,"创建自己的观察者: %s",observer_self_path);
	int observer_self_descriptor = open(observer_self_path, O_RDONLY);
	if (observer_self_descriptor == -1){
		observer_self_descriptor = open(observer_self_path, O_CREAT, S_IRUSR | S_IWUSR);
	}
	int observer_daemon_descriptor = open(observer_daemon_path, O_RDONLY);
	while (observer_daemon_descriptor == -1){
	    LOGI(TAG,"循环读取对方的观察者: %s", observer_daemon_path);
		usleep(1000);
		observer_daemon_descriptor = open(observer_daemon_path, O_RDONLY);
	}
	LOGI(TAG,"删除对方的观察者 %s：告知对方自己已经加锁完成: %s",observer_daemon_path);
	remove(observer_daemon_path);
	LOGI(TAG,"Watched >>>>OBSERVER<<<< has been ready...");
}


/**
 *  Lock the file, this is block method.
 */
int lock_file(char* lock_file_path){
    LOGI(TAG,"start try to lock file >> %s <<", lock_file_path);
    int lockFileDescriptor = open(lock_file_path, O_RDONLY);
    if (lockFileDescriptor == -1){
        lockFileDescriptor = open(lock_file_path, O_CREAT, S_IRUSR);
    }
    int lockRet = flock(lockFileDescriptor, LOCK_EX);
    if (lockRet == -1){
       LOGI(TAG,"lock file failed >> %s <<", lock_file_path);
        return 0;
    }else{
       LOGI(TAG,"lock file success  >> %s <<", lock_file_path);
        return 1;
    }
}


JNIEXPORT void JNICALL Java_com_marswin89_marsdaemon_nativ_NativeDaemonAPI21_doDaemon(JNIEnv *env, jobject jobj, jstring indicatorSelfPath, jstring indicatorDaemonPath, jstring observerSelfPath, jstring observerDaemonPath){
	if(indicatorSelfPath == NULL || indicatorDaemonPath == NULL || observerSelfPath == NULL || observerDaemonPath == NULL){
		LOGI(TAG,"parameters cannot be NULL !");
		return ;
	}

	char* indicator_self_path = (char*)(*env)->GetStringUTFChars(env, indicatorSelfPath, 0);
	char* indicator_daemon_path = (char*)(*env)->GetStringUTFChars(env, indicatorDaemonPath, 0);
	char* observer_self_path = (char*)(*env)->GetStringUTFChars(env, observerSelfPath, 0);
	char* observer_daemon_path = (char*)(*env)->GetStringUTFChars(env, observerDaemonPath, 0);

	int lock_status = 0;
	int try_time = 0;
	while(try_time < 3 && !(lock_status = lock_file(indicator_self_path))){
		try_time++;
		LOGI(TAG,"锁自己的标示：Persistent lock myself failed and try again as %d times", try_time);
		usleep(10000);
	}
	if(!lock_status){
		LOGI(TAG,"锁自己的标示失败：Persistent lock myself failed and exit");
		return ;
	}
	LOGI(TAG,"锁自己的标示: %s",indicator_self_path);

//	notify_daemon_observer(observer_daemon_path);
//	waitfor_self_observer(observer_self_path);
	notify_and_waitfor(observer_self_path, observer_daemon_path);

   LOGI(TAG, "当前进程（service1）ID：%d,实际用户UID为：%d,有效用户UID为：%d,进程组ID：%d",getpid(),getuid(),geteuid(),getpgrp());
   LOGI(TAG,"阻塞式读取对方的锁：");
	lock_status = lock_file(indicator_daemon_path);
	if(lock_status){
		LOGI(TAG,"观察到对方死亡");
		LOGI(TAG,"删除自己的观察者,防止干扰下次启动，并回调java层: %s", observer_self_path);
		remove(observer_self_path);// it`s important ! to prevent from deadlock
		java_callback(env, jobj, DAEMON_CALLBACK_NAME);
	}

}
