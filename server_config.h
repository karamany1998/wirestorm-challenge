
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
#define DST_PORT        44444   
#define SOURCE_PORT     33333
#define MAX_CONNECTIONS 20
#define HEADER_LEN      8
#define EVENT_TIMEOUT   30000


/*
*  This struct will hold important variables related to the 
*  connection between the server and the client such as:
*  1-socket to communicate on
*  2-buffer
*  3-server address
*/
typedef struct connection
{
    int thread_num;
    int currSocket;
    int message_length;
    /*
    all threads will share the same message that's allocated on the heap
    (for the purposes of this program, I think this is OK because I wanted to save space and we only send to destinations without
    modifying it).
    */
    char* msg; 
}   connection_parameters;