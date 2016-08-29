/*
 * File        : daemon_api21.c
 * Author      : Mars Kwok
 * Date        : Jul. 21, 2015
 * Description : This is used to watch process dead over api 21
 *
 * Copyright (C) Mars Kwok<Marswin89@gmail.com>
 *
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
//用于修改进程名
#include <sys/prctl.h>

#include "log.h"
#include "constant.h"

#define TAG ("YHF-Server21")

/**
 *  get the android version code
 */
int get_version(){
	char value[8] = "";
    __system_property_get("ro.build.version.sdk", value);
    return atoi(value);
}
char *str_stitching(const char *str1, const char *str2, const char *str3){
	char *result;
	result = (char*) malloc(strlen(str1) + strlen(str2) + strlen(str3) + 1);
	if (!result){
		return NULL;
	}
	strcpy(result, str1);
	strcat(result, str2);
	strcat(result, str3);
    return result;
}

/**
 * start a android service
 */
void start_service(char* package_name, char* service_name){
    pid_t pid = fork();
    if(pid < 0){
        LOGI(TAG, "fork失败");
    }else if(pid == 0){
        if( setsid()==-1){
         LOGI(TAG, "setsid失败");
        };
        LOGI(TAG, "二进制进程中，启动子进程ID：%d,实际用户UID为：%d,有效用户UID为：%d,进程组ID：%d",getpid(),getuid(),geteuid(),getpgrp());
        LOGI(TAG, "二进制进程中，启动子进程，启动服务后退出");
        if(package_name == NULL || service_name == NULL){
            exit(0);
        }
        umask(0);;//【4】重设文件创建掩模
        int version = get_version();
        char* pkg_svc_name = str_stitching(package_name, "/", service_name);
        if (version >= 17 || version == 0) {
            LOGI(TAG, "am startservice  --user 0 -n %s --include-stopped-packages", pkg_svc_name);
            if(execlp("am", "am", "startservice", "--user", "0","-n", pkg_svc_name,"--include-stopped-packages", (char *) NULL)==-1){
               LOGI(TAG, "am startservice  启动失败: %s",strerror(errno));
            };
            //execlp("/data/data/com.marswin89.marsdaemon.demo/app_bin/daemon21",
            //      "/data/data/com.marswin89.marsdaemon.demo/app_bin/daemon21",
            //      "pkg_name", package_name,
            //       "svc_name", service_name,
            //        "indicator_self_path", "s",
            //        "indicator_daemon_path",  "s",
            //         "observer_self_path",  "s",
            //         "observer_daemon_path",  "s",
            //         (char *) NULL);
            //execlp("am", "am", "start ", "--user", "0", "-n", "com.android.camera/com.android.camera.Camera", (char *) NULL);
            // execlp("am", "am", "start", "-n", "com.marswin89.marsdaemon.demo/com.marswin89.marsdaemon.demo.MainActivity", (char *) NULL);
             //execlp("am", "am", "startservice", "--user", "0","-n", "com.marswin89.marsdaemon.demo/com.marswin89.marsdaemon.demo.Service3", (char *) NULL);
        }
         else {
            LOGI(TAG, "am startservice -n %s", pkg_svc_name);
            execlp("am", "am", "startservice", "-n", pkg_svc_name, (char *) NULL);
        }
        exit(0);
    }else{
        waitpid(pid, NULL, 0);
    }
}

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


void doDaemon(char* indicator_self_path, char* indicator_daemon_path, char* observer_self_path, char* observer_daemon_path,char*pkg_name,char* svc_name){
	if(indicator_self_path == NULL || indicator_daemon_path == NULL || observer_self_path == NULL || observer_daemon_path == NULL){
		LOGI(TAG,"doDaemon----parameters cannot be NULL !");
		return ;
	}

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
   LOGI(TAG,"阻塞式读取对方的锁：.......");
	lock_status = lock_file(indicator_daemon_path);
	if(lock_status){
		LOGI(TAG,"观察到对方死亡");
		LOGI(TAG,"删除自己的观察者,防止干扰下次启动，并启动service: %s", observer_self_path);
		remove(observer_self_path);// it`s important ! to prevent from deadlock
		//java_callback(env, jobj, DAEMON_CALLBACK_NAME);  TODO  下一步考虑怎么启动对方
		//execlp("am", "am", "startservice", "--user", "0","-n", "com.marswin89.marsdaemon.demo/com.marswin89.marsdaemon.demo.Service3", (char *) NULL);
		int count = 0;
        while(count < 50){ //循环启动50次
           LOGI(TAG,"第%d次循环启动服务......",count+1);
            start_service(pkg_name, svc_name);//二进制文件监听到父进程死掉，直接用c代码发intent，这里启动的是 “辅助服务进程”
        	usleep(1000);//1s——usleep功能把进程挂起一段时间， 单位是微秒（百万分之一秒） 100000
        	count++;
        }
	}

}

int main(int argc, char *argv[]){
  LOGI(TAG,"进入二进制的 守护进程；");

    //这里用fork是为了让他的父进程id好看一些，别无他意

        //LOGI(TAG, "第二个子进程ID：%d,实际用户UID为：%d,有效用户UID为：%d,进程组ID：%d",getpid(),getuid(),geteuid(),getpgrp());
		//setsid();
		// LOGI(TAG, "第二个子进程setsid：%d,实际用户UID为：%d,有效用户UID为：%d,进程组ID：%d",getpid(),getuid(),geteuid(),getpgrp());

		char* pkg_name;
		char* svc_name;
		 char* indicator_self_path;
		 char* indicator_daemon_path ;
		 char* observer_self_path  ;
		 char* observer_daemon_path;

		if(argc < 13){
			LOGI(TAG,"daemon parameters error: %d", argc);
			return ;
		}
			int i;
        		for (i = 0; i < argc; i ++){
        		 printf("%s\n",argv[i]);
        			if(argv[i] == NULL){
        				continue;
        			}
        			if (!strcmp("pkg_name", argv[i])){
        				pkg_name = argv[i + 1];
        			}else if (!strcmp("svc_name", argv[i]))	{
        				svc_name = argv[i + 1];
        			}else if (!strcmp("indicator_self_path", argv[i])){
        				indicator_self_path = argv[i + 1];
        			}else if (!strcmp("indicator_daemon_path", argv[i]))	{
        				indicator_daemon_path = argv[i + 1];
        			}else if (!strcmp("observer_self_path", argv[i]))	{
        				observer_self_path = argv[i + 1];
        			}else if (!strcmp("observer_daemon_path", argv[i]))	{
        				observer_daemon_path = argv[i + 1];
        			}
        		}

         LOGI(TAG,"二进制的守护进程,命令参数：%s-----%s-----%s-----%s-----%s-----%s", pkg_name,svc_name,indicator_self_path,indicator_daemon_path,observer_self_path,observer_daemon_path);
        	doDaemon(indicator_self_path,indicator_daemon_path, observer_self_path,observer_daemon_path,pkg_name,svc_name);


}
