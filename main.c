

#include "socket_setup.h"
/* This function serves as the connection handler for destination clients
*  Each destination client will be a thread that will use this function to receive
*  messages from the server
*
*/
void *receiver_handler(void* receiver_connection )
{
    connection_parameters* currConnection = (connection_parameters* )receiver_connection;
    
    int msg_len = currConnection->message_length;
    int total_sent = 0 ; 
    int len_sent = 0 ; 

    //first we can to receive the number of bytes in the message.
    
    while(msg_len>0)
    {
#ifdef TEST_LOCAL
        len_sent = send(currConnection->currSocket , &(currConnection->msg[total_sent]), msg_len - total_sent , 0);
#else
         len_sent = send(currConnection->currSocket , &(currConnection->msg[total_sent]) , msg_len - total_sent, 0);
#endif
        if (len_sent <= 0) {
            //printf("send() failed or returned 0 on socket %d \n", currConnection->currSocket );
            break;
        }
        msg_len -= len_sent;
        total_sent+= len_sent;

        printf("sent %i bytes to client \n" , len_sent);
        printf("remaining to send is %i \n", msg_len);
    }

    printf("sending data on socket %i \n ", currConnection->currSocket);
    printf("data to send is: %s \n" , currConnection->msg);

    printf("----------------------------------------\n");
    return ;  
}

/*
* An  array of threads. 
* Each of which will handle a connection between the server and a reciever client.
*/
pthread_t receiver_thread[MAX_CONNECTIONS];

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
    connection_parameters receiver_struct_arr[MAX_CONNECTIONS];
    for(int i = 0 ; i < MAX_CONNECTIONS ; i++)
    {
        memset(&receiver_struct_arr[i] , 0 , sizeof(connection_parameters));
    }

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
                    if(receiver_struct_arr[j].currSocket==0)
                    {
                        receiver_struct_arr[j].currSocket = client_connection;
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
                //copy message over to each dst struct
                int dstCnt = 0;
                for(int j = 0 ; j<MAX_CONNECTIONS  ; j++)
                {
                    if(receiver_struct_arr[j].currSocket==0)continue;
                    //memset(receiver_struct_arr[j].msg , 0 , 8200);
                    
                    //memcpy(receiver_struct_arr[j].msg , src_msg , msg_len);
                    receiver_struct_arr[j].msg = src_msg;
                    receiver_struct_arr[j].message_length = msg_len ;
                    
                    /* once we receive a message from the source client.
                    * We can create a thread for each destination client to send 
                    * the message
                    */
                    int ret = pthread_create(&receiver_thread[j], NULL, receiver_handler, &receiver_struct_arr[j]);
                    if(ret == -1)
                    {
                        printf("failed to create thread....\n");
                        return -1;
                    }
                    
                }
                
                for(int j = 0 ; j<MAX_CONNECTIONS; j++)
                {
                    if(receiver_struct_arr[j].currSocket ==0)continue;

                    pthread_join(receiver_thread[j] , NULL);
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
                    if(receiver_struct_arr[j].currSocket == dst_fd)
                    {
                        printf("found dest in thread pool \n");
                        receiver_struct_arr[j].currSocket = 0 ;

                        epoll_ctl(epoll_instance_fd , EPOLL_CTL_DEL , dst_fd , NULL);
                        break;
                    }
                }
                printf("------------------------------------\n");

            }
        }
    }

    if(src_msg != NULL)free(src_msg);
    return 0 ;
}