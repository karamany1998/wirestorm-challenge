
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/epoll.h>

#define DST_PORT    44444   
#define SOURCE_PORT  33333
#define MAX_CONNECTIONS 100

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
    char msg[8200];

}   connection_parameters;




/* This function serves as the connection handler for destination clients
*  Each destination client will be a thread that will use this function to receive
*  messages from the server
*
*/
void *receiver_handler(void* receiver_connection)
{
    connection_parameters* currConnection = (connection_parameters* )receiver_connection;
    
    int msg_len;
    //first we can to receive the number of bytes in the message.
    msg_len = send(currConnection->currSocket , currConnection->msg , 5 , 0);
    if(msg_len == -1)
    {
        printf("did not send properly to destination client... \n");
        return ;
    }

    return ;  
}

 

/*
* An  array of threads. 
* Each of which will handle a connection between the server and a reciever client.
*/
pthread_t receiver_thread[5];

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

    /*
     * 1 magic byte + 1 padding byte + 2 byte length + 4 padding bytes 
     * + maximum 8192 data bytes(because 2^16 / 8) = 8200 bytes
    */
    char message[8200];
    memset(message , 0 , 8200);

    /*
     * The socket system call returns an integer which is just a file descriptor
     * that defines the listening socket.
     * AF_INET = ipv4
     * SOCK_STREAM = tcp
    */
    listening_socket_destination = socket(AF_INET , SOCK_STREAM , 0); 
    if (listening_socket_destination == -1)
    {
        printf("Error in socket() system call...\n");
        return -1;
    }
    printf("server destination socket created successfully \n ");

    listening_socket_source = socket(AF_INET , SOCK_STREAM , 0); 
    if (listening_socket_source == -1)
    {
        printf("Error in socket() system call...\n");
        return -1;
    }
    printf("server source socket created successfully \n ");
    printf("--------------------------------------\n");


    memset(&server_address_destination , 0 , sizeof(server_address_destination));
    server_address_destination.sin_family = AF_INET;
    server_address_destination.sin_port = htons(DST_PORT);
    server_address_destination.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(listening_socket_destination , (struct sockaddr*)&server_address_destination , sizeof(server_address_destination)) == -1)
    {
        printf("cannot bind socket to ip/port pair \n");
        return -1;
    }
    printf("destination socket successfully bind-ed to destination port \n");

    memset(&server_address_source, 0 , sizeof(server_address_source));
    server_address_source.sin_family = AF_INET;
    server_address_source.sin_port = htons(SOURCE_PORT);
    server_address_source.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(listening_socket_source , (struct sockaddr*)&server_address_source , sizeof(server_address_source)) == -1)
    {
        printf("cannot bind socket to ip/port pair \n");
        return -1;
    }
    printf("source socket successfully bind-ed to destination port \n");


   

     printf("listening for incoming destination client connections... \n");
    /* listen for  incoming connections from the destination clients */
    if (listen(listening_socket_destination , 5) == -1)
    {
        printf("error listening on the destination client socket \n ");
        return -1;
    }
    printf("listening for incoming destination client connections... \n");


   
    /* listen for  incoming connections from the destination clients */
    if (listen(listening_socket_source , 1) == -1)
    {
        printf("error listening on the source client socket \n ");
        return -1;
    }
    printf("listening for incoming source client connections... \n");

    printf("-----------------------------------------\n");


    /* To make a responsive server that can handle connections and messages from 
     *  the source clientasynchronously, I will utilise epoll. I will add the following 
     * sockets to the epoll instance's "interest set"
     * 1- listenting_socket_source
     * 2- listenting_socket_destination
     * 3- socket that accept() returns with the source client listening socket
     * 
     * Also, this will allow me to be able to handle connection requests from destination clients in a more
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
    struct epoll_event incoming_events[MAX_CONNECTIONS];

    /* we need to specify the sockets(file descriptors) that the 
    * epoll instance will monitor and also specify some flags
    * this is done by passing epoll_event structs using the epoll_ctl() system call
    */
    source_client_config.data.fd=listening_socket_source;
    destination_client_config.data.fd = listening_socket_destination;
        
    source_client_config.events = EPOLLIN;
    destination_client_config.events = EPOLLIN;
    source_client_connection_config.events = EPOLLIN;
    

    int retVal = epoll_ctl(epoll_instance_fd, EPOLL_CTL_ADD, listening_socket_source, &source_client_config);
    int retVal2 = epoll_ctl(epoll_instance_fd, EPOLL_CTL_ADD, listening_socket_destination, &destination_client_config);

    if(retVal == -1 || retVal2 == -1)
    {
        printf("could not add to epoll interest set...Exiting..\n");
        return -1;
    }

    printf("event loop started...\n");

    int src_client_socket = -1;
    int num_dst_clients = 0 ; 
    connection_parameters receiver_struct_arr[MAX_CONNECTIONS];

    while(1)
    {

        /*
        * set a 15 second timeout and wait for events
        */
        int num_events = epoll_wait(epoll_instance_fd, incoming_events, MAX_CONNECTIONS, -1);

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
                receiver_struct_arr[num_dst_clients].currSocket = client_connection;
                
                
                num_dst_clients++;
                printf("number of connected destination clients is %i \n", num_dst_clients);

            }
            else if(incoming_events[i].data.fd == src_client_socket)
            {
                printf("received a message from the source client..\n");
    
                char src_msg[8200];
                int msg_len;
                memset(src_msg, 0 , 8200);
                //first we can to receive the number of bytes in the message.
                msg_len = recv(src_client_socket , src_msg , 8200 , 0);

                if(msg_len == -1)
                {   
                    printf("did not receive properly... \n");
                    return -1;
                }
                printf("message length is %i \n ", msg_len);
                printf("received message: %s from source client \n", src_msg);

                //copy message over to each dst struct
                for(int j = 0 ; j<num_dst_clients ; j++)
                {
                    memset(receiver_struct_arr[j].msg , 0 , 8200);
                    strcpy(receiver_struct_arr[j].msg , src_msg);
                }
                /* once we receive a message from the source client.
                * We can create a thread for each destination client to send 
                * the message
                */
                for(int j = 0 ; j <num_dst_clients ; j++)
                {
                    
                    int ret = pthread_create(&receiver_thread[j], NULL, receiver_handler, &receiver_struct_arr[j]);
                    if(ret == -1)
                    {
                        printf("failed to create thread....\n");
                            return -1;
                    }
                }

                for(int j = 0 ; j<5 ; j++)
                {
                    pthread_join(receiver_thread[j] , NULL);
                }
        }

    }
    
    
    }


    return 0 ;
}