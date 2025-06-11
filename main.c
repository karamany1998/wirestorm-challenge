

#include "message_validation.h"

extern int setup_listening_socket(struct sockaddr_in* );
extern uint16_t compute_checksum(char* input , uint32_t len);
extern uint16_t validate_length(char* msg , uint16_t msg_len);
extern uint16_t validate_magic(char* msg);
extern uint16_t validate_checksum(char* msg , uint16_t msg_len);

//will map and index to an available destination socket
//number>0 means that it's available
uint16_t available_dest_socket[MAX_CONNECTIONS];

pthread_t worker_threads[WORKER_THREADS_NUM];


//this is just done for debug purposes, each task will get a unique sequence number
//to see how the main thread inserts and how the worker threads consume the tasks
uint32_t sequence_number = 0 ; 

int main()
{
    
    int listening_socket_source;
    int listening_socket_destination;
    int client_connection;
    int dst_client_connection;
    struct sockaddr_in server_address_source;
    struct sockaddr_in server_address_destination;
    struct sockaddr_in source_client;
    struct sockaddr_in dst_client;
    socklen_t addr_size_source;
    socklen_t addr_size; 
    char* src_msg = malloc(100000);
    pthread_mutex_init(&queue_lock , NULL);
    pthread_cond_init(&queue_condition, NULL);

    printf("setting qu to NULL \n");
    qu = malloc(sizeof(thread_queue));
    qu->firstNode =  NULL;
    qu->lastNode  = NULL;
    numTask = 0 ;

    memset(&server_address_source , 0 , sizeof(server_address_source));
    server_address_source.sin_family = AF_INET;
    server_address_source.sin_port = htons(SOURCE_PORT);
    server_address_source.sin_addr.s_addr = inet_addr("127.0.0.1");

    memset(&server_address_destination , 0 , sizeof(server_address_destination));
    server_address_destination.sin_family = AF_INET;
    server_address_destination.sin_port = htons(DST_PORT);
    server_address_destination.sin_addr.s_addr = inet_addr("127.0.0.1");

    listening_socket_source = setup_listening_socket(&server_address_source);
    listening_socket_destination = setup_listening_socket(&server_address_destination);

    for(int i = 0; i< WORKER_THREADS_NUM ; i++)
    {
        pthread_create(&worker_threads[i], NULL, receiver_handler , NULL);
    }

    for(int i = 0 ; i<MAX_CONNECTIONS ; i++)available_dest_socket[i]=0;

    /* listen for  incoming connections from the destination clients */
    if (listen(listening_socket_destination , 1000) == -1)
    {
        printf("error listening on the destination client socket \n ");
        return -1;
    }
    printf("listening for incoming destination client connections... \n");
    /* listen for  incoming connections from the source clients */
    if (listen(listening_socket_source , 1000) == -1)
    {
        printf("error listening on the source client socket \n ");
        return -1;
    }
    printf("listening for incoming source client connections... \n");
    printf("-----------------------------------------\n");


    /* To make a responsive server that can handle connections and messages from 
     *  the source client asynchronously, I will utilise epoll. I will add the following 
     * sockets to the epoll instance's "interest set"
     * 1- listenting_socket_source
     * 2- listenting_socket_destination
     * 3- socket that accept() returns with the source client listening socket
     * 4- socket that accept() returns with destination client listening socket
     * 
     * This will allow me to handle connection requests from destination clients in a more
     * interactive manner. Meaning, I don't have to call accept() first for all destination clients, then 
     * send them the message from the source, but I can have, for example, 3 connections, send the data from source, then
     * add other destination clients, send more data and so on. 
     * 
     * tldr: epoll is good because we'll make the server event-driven
     */

    int epoll_instance_fd = epoll_create1(0);
    if(epoll_instance_fd == -1)
    {
        printf("could not create the epoll instance...\n");
        return -1;
    }

    printf("epoll instance created successfully....\n");
    struct epoll_event source_client_config;
    struct epoll_event destination_client_config;
    struct epoll_event source_client_connection_config;
    struct epoll_event destination_client_connection_config[MAX_CONNECTIONS];
    struct epoll_event incoming_events[MAX_CONNECTIONS];

    /* we need to specify the sockets(file descriptors) that the 
    * epoll instance will monitor and also specify some flags
    * this is done by passing epoll_event structs using the epoll_ctl() system call
    */
    source_client_config.data.fd=listening_socket_source;
    destination_client_config.data.fd = listening_socket_destination;
        
    //source_client_config.events = EPOLLIN|EPOLLET;
    source_client_config.events = EPOLLIN;
    //destination_client_config.events = EPOLLIN | EPOLLET;
    destination_client_config.events = EPOLLIN ;
    //source_client_connection_config.events = EPOLLIN | EPOLLET;
    source_client_connection_config.events = EPOLLIN ;
    for(int i = 0 ; i<MAX_CONNECTIONS ; i++)
    {
        destination_client_connection_config[i].events = EPOLLIN | EPOLLET;
    }

    int retVal = epoll_ctl(epoll_instance_fd, EPOLL_CTL_ADD, listening_socket_source, &source_client_config);
    int retVal2 = epoll_ctl(epoll_instance_fd, EPOLL_CTL_ADD, listening_socket_destination, &destination_client_config);
    if(retVal == -1 || retVal2 == -1)
    {
        printf("could not add to epoll interest set...Exiting..\n");
        return -1;
    }
    
    int src_client_socket = -1;
    int num_dst_clients = 0 ; 
    

    printf("event loop started...\n");
    while(1)
    {
        /*
        * set timeout to EVENT_TIMEOUT
        * if timeout then program will exit
        */
        printf("---------------------------------------------\n");
        int num_events = epoll_wait(epoll_instance_fd, incoming_events, MAX_CONNECTIONS, EVENT_TIMEOUT);

        if(num_events == 0)
        {
            printf("we timed out...Exiting event-loop \n");
            break;
        }

        printf("received %i incoming events \n", num_events);
        for(int i = 0 ; i<num_events ; i++)
        {
            if(incoming_events[i].data.fd == listening_socket_source)
            {
                printf("received an incoming connection request from the source client..\n");
                addr_size = sizeof(source_client);
                src_client_socket = accept(listening_socket_source , (struct sockaddr*)&source_client , (socklen_t *)&addr_size);
                if(src_client_socket == -1)
                {
                    printf("failed to connect to source..Exiting \n");
                    return -1;
                }
                
                printf("connected to source successully..source socket file descriptor is %i \n", src_client_socket);
                printf("Adding source socket to the epoll interest set to handle incoming messages.. \n");
                source_client_connection_config.data.fd = src_client_socket;
                epoll_ctl(epoll_instance_fd, EPOLL_CTL_ADD, src_client_socket, &source_client_connection_config);
                printf("---------------------------------------------\n");

            }
            else if(incoming_events[i].data.fd == listening_socket_destination)
            {
                printf("received an incoming connection request from a destination client \n");
                addr_size = sizeof(dst_client);
                client_connection = accept(listening_socket_destination , (struct sockaddr*)&dst_client , (socklen_t *)&addr_size);
                if(client_connection == -1)
                {
                    printf("Failed to accept connection \n");
                    return -1;
                }
                printf("accepted a new destination connection on socket %i \n" , client_connection);
                for(int j = 0 ; j<MAX_CONNECTIONS ; j++)
                {
                    if(available_dest_socket[j]==0)
                    {
                        available_dest_socket[j] = client_connection;
                        printf("Adding destination socket to the epoll interest set to handle closing the destination sockets.. \n");
                        destination_client_connection_config[j].data.fd = client_connection;
                        epoll_ctl(epoll_instance_fd, EPOLL_CTL_ADD, client_connection, &destination_client_connection_config[j]);
                        break;
                    }
                }
                num_dst_clients++;
                printf("number of connected destination clients is %i \n", num_dst_clients);

            }
            else if(incoming_events[i].data.fd == src_client_socket)
            {

                if(incoming_events[i].events & EPOLLRDHUP)
                {
                    printf("source socket closed...\n");
                    epoll_ctl(epoll_instance_fd , EPOLL_CTL_DEL, src_client_socket, 0);
                    close(src_client_socket);
                    src_client_socket = -1;
                    continue;
                }
                printf("received a message from the source client..\n");
                
               
                int msg_len;
                //receive message.
                msg_len = recv(src_client_socket , src_msg , 100000, 0);
                
                printf("validating message...\n");
                printf("message length is %i \n ", msg_len);


                if(msg_len == -1)
                {   
                    printf("did not receive properly... \n");
                    continue;
                }
                else if(msg_len == 0)
                {
                    printf("source socket closed...\n");
                    epoll_ctl(epoll_instance_fd , EPOLL_CTL_DEL, src_client_socket, 0);
                    close(src_client_socket);
                    src_client_socket = -1;
                    continue;
                }
#ifndef TEST_LOCAL
                printf("validating CMTP Message.....\n");

                if(!(validate_magic(src_msg) && validate_length(src_msg , msg_len)))
                {
                    continue;
                }
                uint16_t validCheckSum = validate_checksum(src_msg , msg_len);
                if(!validCheckSum)
                {
                    printf("checksum false..dropping message \n");
                    continue;
                }
                
                printf("--------------------------------\n");

#endif
               

#ifndef TEST_LOCAL
                printf("received message is : %s  \n", &src_msg[8]);
#endif

                //construct a task and insert it in the queue
                int dstCnt = 0;
                
                
                for(int j = 0 ; j<MAX_CONNECTIONS ; j++)
                {
                    if(available_dest_socket[j] == 0)continue;
                    
                    task* newTask = malloc(sizeof(task));
                    newTask->dest_socket = available_dest_socket[j];
                    newTask->message_length = msg_len;
                    newTask->msg = malloc(msg_len);
                    newTask->seq_num = sequence_number++;

                    printf("copying message to task.... \n");
                    memcpy(newTask->msg , src_msg, msg_len);
                    node* newNode = malloc(sizeof(newNode));

                    
                    newNode->currTask = newTask;
                    newNode->next = NULL;
                    

                    pthread_mutex_lock(&queue_lock);
                
                        numTask++;
                        printf("In critical section....inserting in queue....\n");
                        insertNode(newNode);

                    pthread_mutex_unlock(&queue_lock);
                    pthread_cond_signal(&queue_condition);
                    printf("main inserted new task to task queue with sequence number %d \n", newTask->seq_num);
                }
                
            }
            else
            {
                /*
                * else here means that an event was generated on a destination socket.
                * In this case, it will probably be that the destination socket closed
                */
                int dst_fd = incoming_events[i].data.fd;
                if(close(dst_fd) == -1)
                {
                continue; 
                }

                num_dst_clients--;
                printf("destination socket with df %i closed...\n" , dst_fd);
                for(int j  = 0 ; j<MAX_CONNECTIONS ; j++)
                {
                    if(available_dest_socket[j] == dst_fd)
                    {
                        printf("found dest in thread pool \n");
                        available_dest_socket[j] = 0 ;

                        epoll_ctl(epoll_instance_fd , EPOLL_CTL_DEL , dst_fd , NULL);
                        break;
                    }
                }
                printf("------------------------------------\n");

            }
        }
    }

    if(src_msg != NULL)free(src_msg);

    pthread_mutex_destroy(&queue_lock);
    pthread_cond_destroy(&queue_condition);

    return 0 ;
}