/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander - initial API and implementation and/or initial documentation
 *******************************************************************************/

#if !defined(MQTTLiteOS_H)
#define MQTTLiteOS_H


#include <sys/types.h>

#if !defined(SOCKET_ERROR)
    /** error in socket operation */
#define SOCKET_ERROR (-1)
#endif


#define INVALID_SOCKET SOCKET_ERROR
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <net/if.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "lwip/ip_addr.h"
#include "lwip/netifapi.h"

#include "lwip/sockets.h"

#define MQTT_TASK

typedef struct Thread {
    osThreadId_t task;
} Thread;

int ThreadStart(Thread* thread, void (*fn)(void*), void* arg);

typedef struct Timer {
    struct timeval end_time;
} Timer;

typedef struct Mutex {
    osSemaphoreId_t sem;
} Mutex;

void MqttMutexInit(Mutex* mutex);
int MqttMutexLock(Mutex* mutex);
int MqttMutexUnlock(Mutex* mutex);

void TimerInit(Timer* timer);
char TimerIsExpired(Timer* timer);
void TimerCountdownMS(Timer* timer, unsigned int);
void TimerCountdown(Timer* timer, unsigned int);
int TimerLeftMS(Timer* timer);

typedef struct Network {
    int my_socket;
    int (*mqttread) (struct Network*, unsigned char*, int, int);
    int (*mqttwrite) (struct Network*, unsigned char*, int, int);
} Network;

int linux_read(Network* n, unsigned char*, int, int);
int linux_write(Network* n, unsigned char*, int, int);

void NetworkInit(Network* n);
int NetworkConnect(Network* n, char*, int);
void NetworkDisconnect(Network* n);

#endif
