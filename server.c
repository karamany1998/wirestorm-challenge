
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
    char* msg; //all threads will share the same message that's allocated on the heap

}   connection_parameters;



/* This functions performs the setup functionality for listening sockets.
 * It will first call the socket() system call to return a fd referencing the socket.
 * Then bind it to a specific ip/port pair which is given as an input
 *
 * Return value: -1 if unsuccessful and the socket fd otherwise
 */
int setup_listening_socket(struct sockaddr_in* addr)
{
     /*
     * The socket system call returns an integer which is just a file descriptor
     * that defines the listening socket.
     * AF_INET = ipv4
     * SOCK_STREAM = tcp
    */
    int sock_fd = socket(AF_INET , SOCK_STREAM , 0);

    if(sock_fd == -1)
    {
        printf("Error in socket() system call...\n");
        return -1;
    }

    if (bind(sock_fd , addr , sizeof(struct sockaddr_in)) == -1)
    {
        printf("cannot bind socket to ip/port pair \n");
        return -1;
    }
    return sock_fd;
}


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



/* This function will compute the checksum of the message received.
 * This occurs when the sensitive bit in the options fields is set.
 */
uint16_t compute_checksum(char* input , uint32_t len)
{
    printf("---------------------------------------\n");
    uint32_t idx = 0; 
    printf("computing checksum.....\n");

    uint32_t checksum = 0 ; 
    uint32_t checksum_host = 0 ; 
    while(idx < len)
    {
        //the problem states that we should fill in checksum field with 0xcc 0xcc when computing it
        //just add it directly cuz I don't want to modify the string here then change it back afterwards
        if(idx == 4)
        {
            checksum += (uint16_t)0xcccc;
            idx+=2;
            continue;
        }
        //handle edge case(one byte left at the end)
        if(idx+1>=len)
        {
            checksum+= (unsigned char)input[idx];
            printf("odd-length string \n");
            break;
        }
        //copy over two chars(16-bit word) starting at index idx and add them to checksum
        //need to consider network-order to host-order so use ntohs
        uint16_t currVal = 0;
        memcpy(&currVal, &input[idx], 2);
        checksum += (currVal);
        idx += 2;
    }
    /*
     * We also need to consider any overflow because we are doing a 
     * one's compelemt sum. 
     * An easy way to do this is to add top-16 bits to 
     * lower 16-bits and keep looping until the top 16 bits are zero
     */

    uint32_t overflow = checksum>>16;
    printf("overflow is %u \n" , overflow);
    printf("lower 16-bits in checksum is %u \n", (uint16_t)(checksum&0xFFFF));
    while(overflow)
    {   
        
        checksum = overflow + (checksum & 0xFFFFU);
        overflow = checksum>>16;
        printf("overflow is %u \n" , overflow);
        printf("lower 16-bits in checksum is %u \n", (uint16_t)(checksum&0xFFFF));
        printf("-----------------------------------\n");
    }
    return ~((uint16_t)checksum);
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
                memset(src_msg, 0 , 100000);
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
                if(msg_len < 8)
                {
                    printf("message is less than 8 bytes(must be atleast >= header size)...Not Sending \n");
                    continue;
                }
                //validate magic byte
                if((unsigned char) src_msg[0] == 0xcc)
                {
                    printf("received correct magic byte \n");
                }
                else 
                {
                    printf("incorrect magic byte..Not sending message \n");
                    continue;
                }

                uint16_t length_network_order = 0;
                memcpy(&length_network_order , &src_msg[2] , 2);
                uint16_t lengthHeader = ntohs( length_network_order);

                printf("total length is %i \n " , lengthHeader);
                if(msg_len - 8 != lengthHeader)
                {
                    printf("length of data is incorrect..\n");
                    printf("not sending...\n");
                    continue;
                }
                else
                {
                    printf("message length is correct..\n");
                }
                /*
                if( (unsigned char) src_msg[1] == 0x00 && 
                    (unsigned char) src_msg[4]==0x00 &&
                    (unsigned char) src_msg[5] == 0x00 &&
                    (unsigned char) src_msg[6] == 0x00 && 
                    (unsigned char) src_msg[7] == 0x00)
                {
                    printf("correct padding \n");
                }
                else
                {
                    printf("incorrect padding..Not sending \n");
                    continue;
                }
                */
                
                //check option field for the sensitive bit
                unsigned char opts = src_msg[1];
                //if 2nd bit is 1, then compute the checksum
                uint16_t check_checksum = (opts & 0b01000000);
                uint16_t real_checksum = 0;
                memcpy(&real_checksum , &src_msg[4] , 2);

                uint16_t checksum = compute_checksum(src_msg, msg_len);
                printf("real checksum network order is %u \n " , real_checksum);
                printf("first checksum byte is %u \n", (unsigned char)(real_checksum&0xFF));
                printf("second checksum byte is %u \n", (unsigned char)((real_checksum>>8)&0xFF));
                printf("options field is %u \n " , opts);
                printf("sensitive bit is %u \n " , check_checksum);
                printf("checksum val is %u \n ", checksum);
                
                printf("--------------------------------\n");

                //src_msg[2] and src_msg[3] are the length in network byte order
                printf("first len header byte is %c \n" ,  (unsigned char) src_msg[2]);
                printf("second len header byte is %c \n " , (unsigned char) src_msg[3]);
               
               

                if(check_checksum)
                {
                    printf("need to calculate checksum....\n");
                    printf("checksum is %u \n" , checksum);
                    if(checksum != real_checksum)
                    {
                        printf("An error occurred in the message \n");
                        printf("dropping..not sending due to checksum error \n");
                        continue;
                    }
                    else
                    {
                        printf("checksum is correct....\n");
                    }
                }
                else 
                {
                    printf("no need to calculate checksum...\n");
                }
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

    if(src_msg!=NULL)
    free(src_msg);
    return 0 ;
}