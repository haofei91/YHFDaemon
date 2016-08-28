/*
 * File        : daemon_below20.c
 * Author      : Guoyang3
 * Date        : Aug. 14, 2015
 * Description : for easy log.
 */


#include <jni.h>
#include <android/log.h>

#define TAG		"Daemon"

#ifndef LOGHELPER_H_
#define LOGHELPER_H_

// 调试开关
#define DEBUG_ON 1

#if DEBUG_ON

#define LOGI(tag, fmt, ...) __android_log_print(ANDROID_LOG_INFO, (tag), (fmt), ## __VA_ARGS__)
#define LOGD(tag, fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, (tag), (fmt), ## __VA_ARGS__)
#define LOGE(tag, fmt, ...) __android_log_print(ANDROID_LOG_ERROR, (tag), (fmt), ## __VA_ARGS__)

#else

#define LOGI(tag, fmt, ...)
#define LOGD(tag, fmt, ...)
#define LOGE(tag, fmt, ...)

#endif

#endif /* LOGHELPER_H_ */
