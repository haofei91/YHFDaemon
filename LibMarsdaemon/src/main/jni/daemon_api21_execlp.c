/*
 * File        : daemon_api21.c
 * Author      : Mars Kwok
 * Date        : Jul. 21, 2015
 * Description : This is used to watch process dead over api 21
 * 测试二： 2个服务进程中fork子进程，并子进程开启二进制
 *
 * proces1         主         process2            java层  避免了同组问题
 * 二进制子守护1              二进制子守护2
 *
 * 二进制子进程实现相互监听，但是遇到的问题 am命令不能启动service
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/inotify.h>
//用于修改进程名
#include <sys/prctl.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>


#include "com_marswin89_marsdaemon_nativ_NativeDaemonAPI21.h"
#include "log.h"
#include "constant.h"

#define TAG ("YHF-Client21")

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

JNIEXPORT void JNICALL Java_com_marswin89_marsdaemon_nativ_NativeDaemonAPI21_doDaemon(JNIEnv *env, jobject jobj,jstring ABPath, jstring indicatorSelfPath, jstring indicatorDaemonPath, jstring observerSelfPath, jstring observerDaemonPath,jstring pkgName, jstring svcName, jstring daemonPath){
	if(ABPath==NULL ||indicatorSelfPath == NULL || indicatorDaemonPath == NULL || observerSelfPath == NULL || observerDaemonPath == NULL|| pkgName==NULL || svcName==NULL || daemonPath==NULL){
		LOGI(TAG,"parameters cannot be NULL !");
		return ;
	}

	char *pkg_name = (char*)(*env)->GetStringUTFChars(env, pkgName, 0);
	char *svc_name = (char*)(*env)->GetStringUTFChars(env, svcName, 0);
	char *daemo_path21 = (char*)(*env)->GetStringUTFChars(env, daemonPath, 0);
	char *s_work_dir = (char*)(*env)->GetStringUTFChars(env, ABPath, 0);
	char* indicator_self_path = (char*)(*env)->GetStringUTFChars(env, indicatorSelfPath, 0);
	char* indicator_daemon_path = (char*)(*env)->GetStringUTFChars(env, indicatorDaemonPath, 0);
	char* observer_self_path = (char*)(*env)->GetStringUTFChars(env, observerSelfPath, 0);
	char* observer_daemon_path = (char*)(*env)->GetStringUTFChars(env, observerDaemonPath, 0);

	//清理僵尸进程，就像最开始讲的，低端手机会忽略c进程
    //如果我们恰巧运行在低端手机上，那么c进程得不到释放会越来越多，我们称他为僵尸进程，需要清理一下
    	//kill_zombie_process(daemo_path21); //不要去请了，本来就会两个

    pid_t pid;
    int status;
     pid = fork();
     if(pid<0){
       LOGI(TAG, "第一次fork()失败");
     }else if (pid == 0) {
           if(setsid() == -1){ LOGI(TAG, "第一个子进程独立到新会话：失败");}//;//【2】第一子进程成为新的会话组长和进程组长
           LOGI(TAG, "第一个进程ID：%d,实际用户UID为：%d,有效用户UID为：%d,进程组ID：%d",getpid(),getuid(),geteuid(),getpgrp());

            if(pid=fork()>0)
                     exit(0);//【2.1】是第一子进程，结束第一子进程
             else if(pid< 0)
                     exit(1);//fork失败，退出

            //进入第二个子进程  实现守护进程
            LOGI(TAG, "第二个进程，实现孤儿进程，ID：%d,实际用户UID为：%d,有效用户UID为：%d,进程组ID：%d",getpid(),getuid(),geteuid(),getpgrp());
            chdir(s_work_dir);
            umask(0);;//【4】重设文件创建掩模
            int i;
            for( i=0;i< 8;++i)//【5】关闭打开的文件描述符  TODO  数目
              close(i);
           LOGI(TAG, "开始运行二进制文件:......");

           execlp(daemo_path21,
      				daemo_path21,
      				"pkg_name", pkg_name,
      				"svc_name", svc_name,
      				"indicator_self_path", indicator_self_path,
      				"indicator_daemon_path", indicator_daemon_path,
      				"observer_self_path", observer_self_path,
      				"observer_daemon_path", observer_daemon_path,
      				(char *) NULL);

      } else {//主进程
        // 等待第一个子进程退出，继续执行   注意这里是process1和process2  不能关闭
       waitpid(pid, &status, 0);//【1】是父进程
      }

}