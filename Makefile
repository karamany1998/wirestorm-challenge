
#the problem with this project is that there' some dependency between the different files,
#so figuring out the target and dependencies was a little tricky(had to draw graphs..)
#I think further refactoring will be solve this problem.


# Build the final server program


server: main.o socket_setup.o thread_utils.o message_validation.o
	gcc -o server main.o socket_setup.o thread_utils.o message_validation.o

main.o: main.c message_validation.h socket_setup.h thread_utils.h
	gcc -c main.c

socket_setup.o: socket_setup.c server_config.h socket_setup.h
	gcc -c socket_setup.c

thread_utils.o: thread_utils.c server_config.h thread_utils.h
	gcc -c thread_utils.c

message_validation.o: message_validation.c message_validation.h server_config.h
	gcc -c message_validation.c 

clean:
	rm -f server *.o
