/*
 * Server File Description:
 * The server will create a TCP socket that listens on a port. The Port will be the last 4
 * digits of my student id: 2648. The server will accept an incoming connection and then
 * create a new thread (using the pthreads library) that will handle the connection. The
 * new thread will read all the data from the client and respond back to it. This is called
 * the acknowledgment. In this case, I will send back the number of read() calls made.
 */
#include <sys/types.h>    // socket, bind 
#include <sys/socket.h>   // socket, bind, listen, inet_ntoa 
#include <netinet/in.h>   // htonl, htons, inet_ntoa 
#include <arpa/inet.h>    // inet_ntoa 
#include <netdb.h>        // gethostbyname 
#include <unistd.h>       // read, write, close 
#include <strings.h>      // bzero 
#include <netinet/tcp.h>  // SO_REUSEADDR 
#include <sys/uio.h>      // writev

#include <iostream>
#include <stdio.h>
#include <cstring>
#include <sys/time.h> // needed for gettimeofday()

using namespace std; // to use cout and endl

struct communicationThreadData {
	int socketDescriptor;
	int numIterations;
};

/*
 * This function is the meat and potatoes of the server program. Once a connection is accepted and a new thread is
 * created to handl the communication and reponse, this function is called. It takes in the thread data created
 * in the main function which has two variables (iterations and the socket descriptor). This function returns to
 * the client the number of times the buffer was read, and prints to the console the amount of time it took in order
 * to read all of the data.
 */
void* genResponse(void* input) {
	int comThread = ((struct communicationThreadData*)input)->socketDescriptor; // socket descriptor of the current communication thread
	int iterations = ((struct communicationThreadData*)input)->numIterations; // number of iterations
	struct timeval start; // time we start reading
	struct timeval end; // time we stop reading

	int count; // number of reads
	int dataRecievingTime; // difference between end time and start time (how long it took to read the data)
	int BUFSIZE = 1500; // number of bytes in the data buffer (assignment spec says to use 1500)

	char databuf[BUFSIZE]; // data buffer we are reading into

	gettimeofday(&start, NULL); // setting the start time to the current time of day with no input for time zone

	// code from spec sheet
	// reads in data from the client into the buffer iteration times (same iteration number as client side)
	for (int i = 0; i < iterations; i++) {
		for (int nRead = 0; nRead += read(comThread, databuf, BUFSIZE - nRead) < BUFSIZE; count++);
	}

	// once we are done reading, store the new time of day in the end time
	gettimeofday(&end, NULL);

	// send acknowledgement (response) back to client
	// this is the value kept in count (the number of reads we performed on the buffer)
	// this is done via the write system call which takes in a file descriptor, the data, and the size of the data
	// in this case, the file descriptor points to a communication link (the socket) which in linux is a file (everything is a file)
	write(comThread, &count, sizeof(count));

	// print out the time it took to read the data to the console
	dataRecievingTime = (((end.tv_sec - start.tv_sec) * 1000000L) + (end.tv_usec - start.tv_usec));
	cout << "data-receiving time = " << dataRecievingTime << " usec" << endl;

	// finally, once the reponse is formulated and sent, and we no longer need to the communication link, close the socket
	// thus, terminating the file representing the socket and opening up that descriptor
	close(comThread);

	exit(0); // not sure what to return with void pointer
}

/*
 * The program will take in 2 arguments. argv[1] will be the port number (2648)
 * argv[2] will be the number of iterations the server will perfrom on "read"
 * from the socket. If the client performs 100 iterations of data transmission,
 * the server should perform at least 100 reads from the socket to completly
 * recieve all data sent by the client. So the value should be the same as the
 * clients iteration argument.
 */
int main(int argc, char** argv) {
	cout << argc << endl;
	char* port = argv[1]; // 2648 (last four of my student id)
	int iterations = atoi(argv[2]);

	// Step 1 - Declare an addrinfo structure (defined in netdb.h), initialize it to zero, 
	// and set its data members to have my port, have an internet family, send streams (not datagrams), and be passive
	struct addrinfo hints; // how we feed the data from the comment above
	memset(&hints, 0, sizeof(struct addrinfo));
	struct addrinfo *result; // a pointer to the first item in the linked list that has our socket info
	hints.ai_family = AF_UNSPEC; // address family of unspec is the socket address of family ipv4 or 6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // I believe this means it is a passive open?

	// Step 2 - Grab the addrinfo guesses linked list and make sure that it could translate
	// call the getaddrinfo function which returns a linked list of addrinfo for this socket.
	// im assuming that this linked list is size one because we should be gauranteed the correct answer?
	// have to dereference result and hints because result is a double pointer and hints is a pointer
	// the first pointer is used to show the address of the variable and the double pointer points to the address of the pointer
	// first argument is set to null this time as we have no IP address
	int canTranslate = getaddrinfo(NULL, port, &hints, &result); // todo: why does the example spec take an int for port but this wants a string?

	// if could successfully translate the address, it will return 0, otherwise, abort
	if (canTranslate != 0) {
		cout << "Could not translate address with given hints and port" << endl;
		exit (EXIT_FAILURE);
	}

	// Step 3 - try to open the socket with each of the guesses until one works. If none do, abort.
	// all socket() does is open a socket file on your computer and return the descriptor for it.
	// if successful, returns the descriptor, if unsuccessful, returns -1
	// socket() doesnt actually do anything yet, its like building the doorframe without connecting it to a wall yet (cant send or recieve bytes)
	struct addrinfo* curr = result; // result is the head of the linked list and curr is each record/ node

	int serverSocket = -1;

	while (curr != NULL) {
		serverSocket = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);

		if (serverSocket == -1) {
			curr = curr->ai_next;
			continue; // couldnt create a socket so try next addressinfo guess
		}

		// if it successfully created a socket, we have to configure it
		// specifically make sure that the listening sockets port can be reused once we create a thread to handle the connection with a new sd
		// to do this, use setsockopt(). Returns -1 if failes and 0 if succeeds. Arguments:
		// 1. The sockets file descriptor
		// 2. The level - the level to manipulate the option
		// 3. An int for option name - the type of option we want to change
		// 4. A void pointer optval - the new value we want to set the option to (we are going to assign it the value 1)
		// 5. Option length - size of previous arguments value
		// Note: void pointers can hold address of any type and can be typcasted to any type. Cannot be derefrenced
		int enable = 1; // the value 1 enables the reuse option
		setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

		// Bind the socket descriptor we just created to the PORT we desire
		// It reserves the port for our current process (whos job is to listen for connections)
		int successfullyBinded = bind(serverSocket, curr->ai_addr, curr->ai_addrlen);

		// if could bind, returns 0, so we can break and use it
		if (successfullyBinded == 0) {
			break;
		}

		// otherwise, close the socket and try the next guess
		close(serverSocket);
	}

	// if serverSocket is still -1, could not create a socket successfully so abort
	if (serverSocket == -1) {
		cout << "Could not create a socket with the given port" << endl;
		exit (EXIT_FAILURE);
	}

	// free the linked list memory
	freeaddrinfo(result);

	// Step 4 - Listen on the socket we just created
	// Tells the OS it is ready to start recieving connections on this port
	// Needs the serverSocket as a parameter, along with a backlog (number of connections it can handle before it refuses connections)
	// in this case it is also the number of threads with their own socket descripters handling each users request
	int listening = listen(serverSocket, 5);

	// if listen returns -1 it means it failed to listen
	if (listening == -1) {
		cout << "Could not successfully listen on the given port" << endl;
		exit (EXIT_FAILURE);
	}

	// displays that it is listening (just for debugging purposes)
	cout << "Listening for client connection requests on port " << port << "!" << endl;

	// Step 5 - Keep the server running indefinently, listening for client connection requests and accepting them
	// this is done via an infinite loop
	// to accept client connections, we use the accept function which takes in
	// 1. the server socket descriptor
	// 2. a pointer for a sockaddr - the client address. Where the accept function will store the location of this
	// 3. a pointer to the length of the variable pointed to by the previous argument
	// returns -1 if cannot accept, and a new file descriptor if it does accept

	while (true) {
		struct sockaddr clientAddress;
		socklen_t clientAddressLength = sizeof(clientAddress);

		int clientSocketDescriptor = accept(serverSocket, &clientAddress, &clientAddressLength);

		cout << "recieved request from client address: " << clientAddress->sa_data << endl;

		if (clientSocketDescriptor == -1) {
			cout << "Failed to accept client connection request" << endl;
			exit(EXIT_FAILURE);
		}

		cout << "created socket for client with descriptor: " << clientSocketDescriptor << endl;

		// if we were successfully able to accept the connection, we should now create a new thread to handle
		// the communication with the current client with the new socket descriptor just created
		pthread_t comThread;
		struct communicationThreadData* data = new communicationThreadData;
		data->socketDescriptor = clientSocketDescriptor;
		data->numIterations = iterations;

		pthread_create(&comThread, NULL, genResponse, (void*) comThread);
	}

	return 0;
}