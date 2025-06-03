

#include "socket_setup.h"

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