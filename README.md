

# Wirestorm Challenge
```bash
Made sure to solve both parts of the challenge :)
Note the file server.c is the "old code" which was functionally correct
but I refactored it into the remaining parts.

The server is event-driven. It first sets up the listening source and destination sockets
and then waits for incoming events.
Note: this means that we can have a source and X destinations
and then for example send messages from source to destination then add other destination clients later on because
the server is flexible due to the usage of epoll and event-driven programming.
When the source client or a destination client establishes a connections,
it will handle it and create a socket for that connection.
When the source sends a message, then the server will receive it and enqueue the message as a task inside a task queue.
then a worker thread will dequeue the task and execute it(i.e send the message to the destination).
The task queue has to be protected by a mutex to avoid any race conditions and also the
main thread will utilise a condition variable to signal that the task queue is not empty.
Worker threads will wait(sleep) until that condition variable is signalled. This is used to not waste cpu resources(no busy waiting).

When a connection ends, then a flag "EPOLLRDHUP" is set, so I used this to remove the fd from the epoll interest set.


```
## Building the project

```bash
1-   make
2-  ./server

the server will then wait for incoming connections and also display debug output on stout

```

```

## Files included

1-socket_utils.h and socket_utils.c : Provide functionality to setup the sockets used for listeing 
for source and destination clients

2-message_validation.h and message_validation.c: Provide the functionality to validate the incoming message
from the source client, including validation the length in the header, checksum calculation and magic byte 
verification

3-server_config.h: Provide includes and #defines for different parameters used throughout the project
such as source and destination ports, header length and MAX_CONNECTIONS..etc

4- main.c: the most exciting part of the project. Used epoll() to make it event-driven and used pthreads to 
handle sending data to the different destinations.

5- thread_utils.h and thread_utils.c: Provides functionality for worker threads and some data structures necessary. Provides some functions to add/remove from
task queue.
6- Makefile: build the project using "make" and then run "./server" (Ez)
