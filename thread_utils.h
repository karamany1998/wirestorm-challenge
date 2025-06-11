




#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H

#include "server_config.h"



//
typedef struct task
{
    uint32_t seq_num;
    uint16_t dest_socket;
    uint16_t message_length;
    char* msg;
} task;


typedef struct node
{
    task* currTask;
    struct node* next;
}node;


typedef struct thread_queue
{

    node* firstNode;
    node* lastNode;
    uint16_t num_nodes;

} thread_queue;



extern void insertNode(node* currNode);
extern node* getNode();
extern void *receiver_handler();

/* An array of worker threads that will handle tasks when 
 * they become available
 */
extern pthread_t worker_threads[WORKER_THREADS_NUM];

/*mutex used to manage access(enqueue and deqeueue) to the task queue*/
extern pthread_mutex_t queue_lock;

/* condition variable that signals when the task queue has tasks(numTasks>0). This is signalled from the main
* thread when a new tasks comes in.
*/
extern pthread_cond_t  queue_condition;

extern int numTask;


extern thread_queue* qu;



#endif