

# Wirestorm Challenge
```bash
Made sure to solve both parts of the challenge :)
Note the file server.c is the "old code" which was functionally correct but I refactored it into the remaining parts.
```
## Building the project

```bash
1-   make
2-  ./server
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
5- Makefile: build the project using "make" and then run "./server" (Ez)
