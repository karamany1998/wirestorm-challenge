
# Build the final server program
server: main.o socket_setup.o message_validation.o
	gcc -o server main.o socket_setup.o message_validation.o


main.o: main.c server_config.h socket_setup.h message_validation.h 
	gcc -c main.c

socket_setup.o: socket_setup.c server_config.h socket_setup.h
	gcc -c socket_setup.c

message_validation.o: message_validation.c server_config.h message_validation.h
	gcc -c message_validation.c

clean:
	rm -f server *.o
