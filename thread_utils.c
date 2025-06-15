#include "thread_utils.h"


int numTask = 0 ; 
pthread_cond_t  queue_condition;
pthread_mutex_t queue_lock;
thread_queue* qu;

/*
*We insert from end of queue
*
*/
void insertNode(node* currNode)
{

    numTask++;
    if(qu->firstNode==NULL && qu->lastNode==NULL)
    {
        printf("qu is empty...\n");
        qu->firstNode = currNode;
        qu->lastNode = currNode;
        return;
    }

    qu->lastNode->next = currNode;
    qu->lastNode = currNode;

    return;
}


/*
* We get nodes from start of queue(LIFO)
*/
node* getNode()
{
    numTask--;
    node* ret = qu->firstNode;
    if(ret == NULL)return NULL;
    if(qu->firstNode == qu->lastNode)
    {
        qu->firstNode = NULL;
        qu->lastNode = NULL;
        return ret;
    }
    qu->firstNode = qu->firstNode->next;
    return ret;
}


/* This function serves as the connection handler for destination clients
* We have a fixed number of worker threads that will keep waiting for tasks to perform
* When a task becomes available in the queue then we will dequeue it and the thread will handle it
*/
void *receiver_handler()
{
    //each worker thread will continuosly keep checking for new tasks to be performed
    while(!exit_condition)
    {   
        task* currTask = NULL;
        pthread_mutex_lock(&queue_lock);

        //wait as long as there're no tasks in the task queue
        while(numTask==0 && !exit_condition) 
        {
            pthread_cond_wait(&queue_condition , &queue_lock);
        }

        if(exit_condition && numTask==0)
        {
            pthread_mutex_unlock(&queue_lock);
            break;
        }

        //at this position we know there's task in the queue and we deque it safely because we have the 
        //lock(which we aquired when we exited the pthread_cond_wait() )
        node* firstElement = getNode();
        if(firstElement == NULL)
        {
            printf("queue is empty...\n");
            continue;
        }
        currTask = firstElement->currTask;
        numTask--;
        printf("thread finished waiting for queue to have task...num task: %d \n" , numTask);
        printf("task has sequence number: %u \n", currTask->seq_num );
        pthread_mutex_unlock(&queue_lock);


        int msg_len = currTask->message_length;
        int len_sent = 0 ; 
        int total_sent = 0 ;
        //first we can to receive the number of bytes in the message.
        
        while(msg_len>0)
        {
   
            len_sent = send(currTask->dest_socket , &(currTask->msg[total_sent]) , msg_len - total_sent, 0);
            if (len_sent <= 0) {
                //printf("send() failed or returned 0 on socket %d \n", currConnection->currSocket );
                break;
            }
            msg_len -= len_sent;
            total_sent+= len_sent;

            printf("sent %i bytes to client \n" , len_sent);
        }

        printf("sending data on socket %i \n ", currTask->dest_socket);

        printf("----------------------------------------\n");

}
    return NULL; 
}