/*
 * File        : daemon_api21.c
 * Author      : Mars Kwok
 * Date        : Jul. 21, 2015
 * Description : This is native process to watch parent process.
 *
 * Copyright (C) Mars Kwok<Marswin89@gmail.com>
 *
 */
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "constant.h"

#define TAG ("YHF-Server")

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
        //error, do nothing...
    }else if(pid == 0){
        LOGI(TAG, "第三个子进程ID：%d,实际用户UID为：%d,有效用户UID为：%d,进程组ID：%d",getpid(),getuid(),geteuid(),getpgrp());
        LOGI(TAG, "第三个子进程，启动服务后退出");
        if(package_name == NULL || service_name == NULL){
            exit(EXIT_SUCCESS);
        }
        int version = get_version();
        char* pkg_svc_name = str_stitching(package_name, "/", service_name);
        if (version >= 17 || version == 0) {
            LOGI(TAG, "am startservice --user 0 -n %s", pkg_svc_name);
            execlp("am", "am", "startservice", "--user", "0", "-n", pkg_svc_name, (char *) NULL);
        } else {
            LOGI(TAG, "am startservice -n %s", pkg_svc_name);
            execlp("am", "am", "startservice", "-n", pkg_svc_name, (char *) NULL);
        }

        exit(EXIT_SUCCESS);
    }else{
        waitpid(pid, NULL, 0);
    }
}



int main(int argc, char *argv[]){

    LOGI(TAG,"进入二进制的 守护进程；");

    //这里用fork是为了让他的父进程id好看一些，别无他意
	pid_t pid = fork();
	if(pid == 0){
	    LOGI(TAG, "第二个子进程ID：%d,实际用户UID为：%d,有效用户UID为：%d,进程组ID：%d",getpid(),getuid(),geteuid(),getpgrp());
		setsid();
		 LOGI(TAG, "第二个子进程setsid：%d,实际用户UID为：%d,有效用户UID为：%d,进程组ID：%d",getpid(),getuid(),geteuid(),getpgrp());
		int pipe_fd1[2];
		int pipe_fd2[2];
		char* pkg_name;
		char* svc_name;
		if(argc < 13){
			LOGI(TAG,"daemon parameters error");
			return ;
		}
		int i;
		for (i = 0; i < argc; i ++){
			if(argv[i] == NULL){
				continue;
			}
			if (!strcmp(PARAM_PKG_NAME, argv[i])){
				pkg_name = argv[i + 1];
			}else if (!strcmp(PARAM_SVC_NAME, argv[i]))	{
				svc_name = argv[i + 1];
			}else if (!strcmp(PARAM_PIPE_1_READ, argv[i])){
				char* p1r = argv[i + 1];
				pipe_fd1[0] = atoi(p1r);
			}else if (!strcmp(PARAM_PIPE_1_WRITE, argv[i]))	{
				char* p1w = argv[i + 1];
				pipe_fd1[1] = atoi(p1w);
			}else if (!strcmp(PARAM_PIPE_2_READ, argv[i]))	{
				char* p2r = argv[i + 1];
				pipe_fd2[0] = atoi(p2r);
			}else if (!strcmp(PARAM_PIPE_2_WRITE, argv[i]))	{
				char* p2w = argv[i + 1];
				pipe_fd2[1] = atoi(p2w);
			}
		}

		close(pipe_fd1[0]);//---------关掉1的读
		close(pipe_fd2[1]);//关掉2的写

		char r_buf[100];
		int r_num;
		memset(r_buf,0, sizeof(r_buf));

        LOGI(TAG,"管道，阻塞式等待父进程死亡中....");
		r_num=read(pipe_fd2[0], r_buf, 100);//持续读2的读,读到证明父进程死亡
		LOGI(TAG,"发现主进程死亡");
		int count = 0;
		while(count < 50){ //循环启动50次
			start_service(pkg_name, svc_name);//二进制文件监听到父进程死掉，直接用c代码发intent，这里启动的是 “辅助服务进程”
			usleep(100000);//1s——usleep功能把进程挂起一段时间， 单位是微秒（百万分之一秒）
			count++;
		}
	}else{
		exit(EXIT_SUCCESS);
	}
}
