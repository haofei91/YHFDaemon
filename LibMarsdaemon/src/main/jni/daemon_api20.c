/*
 * File        : daemon_api21.c
 * Author      : Mars Kwok
 * Date        : Jul. 21, 2015
 * Description : This is used to watch process dead under api 20
 *
 * Copyright (C) Mars Kwok<Marswin89@gmail.com>
 *
 */
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>

#include "log.h"
#include "constant.h"
#include "com_marswin89_marsdaemon_nativ_NativeDaemonAPI20.h"

#define TAG ("YHF-Client20")

static char s_daemon_path[MAX_PATH];

/**
 *  get the process pid by process name
 */
int find_pid_by_name(char *pid_name, int *pid_list){
    DIR *dir;
	struct dirent *next;
	int i = 0;
	pid_list[0] = 0;
	dir = opendir("/proc");
	if (!dir){
		return 0;
	}
	while ((next = readdir(dir)) != NULL){
		FILE *status;
		char proc_file_name[BUFFER_SIZE];
		char buffer[BUFFER_SIZE];
		char process_name[BUFFER_SIZE];

		if (strcmp(next->d_name, "..") == 0){
			continue;
		}
		if (!isdigit(*next->d_name)){
			continue;
		}
		sprintf(proc_file_name, "/proc/%s/cmdline", next->d_name);
		if (!(status = fopen(proc_file_name, "r"))){
			continue;
		}
		if (fgets(buffer, BUFFER_SIZE - 1, status) == NULL){
			fclose(status);
			continue;
		}
		fclose(status);
		sscanf(buffer, "%[^-]", process_name);
		if (strcmp(process_name, pid_name) == 0){
			pid_list[i ++] = atoi(next->d_name);
		}
	}
	if (pid_list){
    	pid_list[i] = 0;
    }
    closedir(dir);
    return i;
}

/**
 *  kill all process by name
 */
void kill_zombie_process(char* zombie_name){
    int pid_list[200];
    int total_num = find_pid_by_name(zombie_name, pid_list);
    LOGI(TAG,"僵尸进程 name is %s, and number is %d, killing...", zombie_name, total_num);
    int i;
    for (i = 0; i < total_num; i ++)    {
        int retval = 0;
        int daemon_pid = pid_list[i];
        if (daemon_pid > 1 && daemon_pid != getpid() && daemon_pid != getppid()){
            retval = kill(daemon_pid, SIGTERM);
            if (!retval){
                LOGI(TAG,"僵尸进程 kill zombie successfully, zombie`s pid = %d", daemon_pid);
            }else{
                LOGI(TAG,"僵尸进程 kill zombie failed, zombie`s pid = %d", daemon_pid);
            }
        }
    }
}
//将对应的packagename，servicename以及二进制可执行文件的路径传进来
JNIEXPORT void JNICALL Java_com_marswin89_marsdaemon_nativ_NativeDaemonAPI20_doDaemon(JNIEnv *env, jobject jobj, jstring pkgName, jstring svcName, jstring daemonPath){
	if(pkgName == NULL || svcName == NULL || daemonPath == NULL){
		LOGI(TAG,"native doDaemon parameters cannot be NULL !");
		return ;
	}

	char *pkg_name = (char*)(*env)->GetStringUTFChars(env, pkgName, 0);
	char *svc_name = (char*)(*env)->GetStringUTFChars(env, svcName, 0);
	char *daemo_path = (char*)(*env)->GetStringUTFChars(env, daemonPath, 0);


//清理僵尸进程，就像最开始讲的，低端手机会忽略c进程
//如果我们恰巧运行在低端手机上，那么c进程得不到释放会越来越多，我们称他为僵尸进程，需要清理一下
	kill_zombie_process(daemo_path);

//建立两条管道 start
	int pipe_fd1[2];//order to watch child
	int pipe_fd2[2];//order to watch parent

	pid_t pid;
	char r_buf[100];
	int r_num;
	memset(r_buf, 0, sizeof(r_buf));
	if(pipe(pipe_fd1)<0){
		LOGI(TAG,"pipe1 create error");
		return ;
	}
	if(pipe(pipe_fd2)<0){
		LOGI(TAG,"pipe2 create error");
		return ;
	}

	char str_p1r[10];
	char str_p1w[10];
	char str_p2r[10];
	char str_p2w[10];

	sprintf(str_p1r,"%d",pipe_fd1[0]);
	sprintf(str_p1w,"%d",pipe_fd1[1]);
	sprintf(str_p2r,"%d",pipe_fd2[0]);
	sprintf(str_p2w,"%d",pipe_fd2[1]);
	LOGI(TAG,"管道建立： 完成");
//建立两条管道 END

     LOGI(TAG, "带守护的主进程（service1）ID：%d,实际用户UID为：%d,有效用户UID为：%d,进程组ID：%d",getpid(),getuid(),geteuid(),getpgrp());
	if((pid=fork())==0){//第一个子进程  5.0以下 fork一次
	//执行二进制文件，将上面的参数传递进去
	//子进程中启动二进制文件，并传递 的包名，服务名，2个管道的读和写
		LOGI(TAG, "第一个子进程，开始运行二进制文件，启动守护进程，当前进程ID：%d,实际用户UID为：%d,有效用户UID为：%d,进程组ID：%d",getpid(),getuid(),geteuid(),getpgrp());
		execlp(daemo_path,
				daemo_path,
				PARAM_PKG_NAME, pkg_name,
				PARAM_SVC_NAME, svc_name,
				PARAM_PIPE_1_READ, str_p1r,
				PARAM_PIPE_1_WRITE, str_p1w,
				PARAM_PIPE_2_READ, str_p2r,
				PARAM_PIPE_2_WRITE, str_p2w,
				(char *) NULL);
	}else if(pid>0){ //父进程
		close(pipe_fd1[1]);//-------关掉管道1的写
		close(pipe_fd2[0]);//关掉管道2的读
		//wait for child
		LOGI(TAG,"管道，阻塞式等待子（守护）进程死亡中....");
		r_num=read(pipe_fd1[0], r_buf, 100);//-------持续读取管道1的读,读到证明子进程死亡
		LOGI(TAG,"读取到子进程，也就是守护进程死亡");
		java_callback(env, jobj, DAEMON_CALLBACK_NAME);//调用java层的onDaemonDead()
	}
}

