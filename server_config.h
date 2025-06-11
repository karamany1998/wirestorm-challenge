

#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <unistd.h>


//used this to make my own connections using netcat to see the program flow and debug output in more detail
//#define   TEST_LOCAL     

//(todo: I added too much run-time debug, so will probably use this to enable/disable the debug output)
//#define     DEBUG_OUTPUT
#define DST_PORT            44444   
#define SOURCE_PORT         33333
#define MAX_CONNECTIONS     20
#define HEADER_LEN          8
#define EVENT_TIMEOUT       30000
#define WORKER_THREADS_NUM  4



#endif