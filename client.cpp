/*
 * This file will create a new socket, connect to the server, and send data uisng
 * three different ways of writing data/ transferring data. Once it does this,
 * it will wait for a response and print it.
 */

// header files provided by professor. Needed to call the OS functions
// windows doesnt have most of these headers provided so need to run this on a linux system
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
#include <cstring> // for memset
#include <sys/time.h> // needed for gettimeofday()

using namespace std;

/*
 * This function returns the head of a linked list of guesses for the socket information.
 * Uses the getaddrinfo() function provided by socket.h to acquire this.
 * Needed because connect() needs specific information about the socket to connect.
 * We feed this function the port number of the process runningo on the server, the ip address
 * of the server, and a set of hints for locating this socket information.
 * getaddrinfo itself returns an integer representing if it worked or not so we feed it the resulting
 * linked list of addrinfo structures.
 */
struct addrinfo* getAddressGuesses (char* portNumber, char* ipAddress) {
    // give the following hints of the server
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // tells the hint that the address family is internet
	hints.ai_socktype = SOCK_STREAM; // tells hint that it is a stream socket protocol
	hints.ai_protocol = IPPROTO_TCP; // TCP isnt the only sock stream protocol so we need to explicitly say TCP

    // the resulting pointer to the first address of the addrinfo linked list (the first guess which points to the next ones)
    struct addrinfo* results;

    // when calling the getaddrinfo() function, it takes hints as a pointer to a addrinfo struct
    // and results as a double pointer. So we need to dereference them with the ampersand

    int status = getaddrinfo(ipAddress, portNumber, &hints, &results);

    // if status is not 0, it means that the function failed, so printing an error
    if (status != 0) {
        cout << "Could not connect to socket with provided port number and ip address" << endl;
        exit (EXIT_FAILURE); // exits the program
    }

    // otherwise, the results variable holds a pointer to the first guess of socket information in the list, so can return it
    return results;
}

/*
 * Returns a socket descriptor to the process running on the server side
 * Loops through the linked list of guesses of socket information and tries to connect to each.
 * If one of them works, returns its descriptor. Head is just the guesses pointer as its the first guess.
 */
int getSocketDescriptor (struct addrinfo* head) {
    struct addrinfo* curr = head;

    int sd = -1; // the return value

    while (curr != NULL) {
        // Step 1 - Create the socket to test with the information from the guess (3 parameters)
        // Parameter 1 - Domain (an integer representing the family (ipv4/ ipv6))
        // Parameter 2 - Type (an integer representing the type (datagram vs. stream))
        // Parameter 3 - Protocol (an integer for the protocol (TCP vs. UDP))

        // socket descriptor points to a index in a table (like a file descriptor)
        // this call is creating that socket and returning the index of that socket in the table
        // this client.cpp program is going to be using this socket
        sd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);

        // if sd is -1, it means it cant connect so try next guess
        if (sd == -1) {
            curr = curr->ai_next;
        } // step 2 - check to see if you can actually connect
        else {
            // if it didnt return -1, it returned 1 meaning success, so check to see if it can actually connect
            // to do that, call connect with three parameters
            // 1. The socket descriptor we just recieved
            // 2. The address information of the socket
            // 3. The length of the address
            int successfulConnection = connect(sd, curr->ai_addr, curr->ai_addrlen);

            // if this is not -1, it means it failed and we should move on
            if (successfulConnection != -1) {
                curr = curr->ai_next;
            }
            else {
                close(sd);
            }
        }
    }

    return sd;
}

/*
 * This function creates a data buffer and uses the write() and writev() sys calls to send write
 * information over to the process running on the server. The type variable describes the type of
 * write we are doing (described over each if statement)
 */
void writeToSocket (int iterations, int nbufs, int bufsize, int type, int socketDescriptor) {
    // allocate a data buffer to send to the server
	// data buffers are temporary storage to transfer between different media and storage
	// basically a array of strings (array of array of characters) with each string sized (bufsize - in bytes)
	// and the number of strings is equivalent to the nbufs (number of data buffers)
	char databuf[nbufs][bufsize]; // where nbufs * bufsize = 1500 (whole load is 1500 bytes)

    // incase it is type 2, need to break the data to segments
    struct iovec vector[nbufs];

	// the assignment says to call the write iteration times in all cases so does this in a loop iteration times
	for (int i = 0; i < iterations; i++) {
		if(type == 1) {
			// if type is 1, we do multiple writes so send each string individually
			for(int j = 0; j < nbufs; j++){
				write(socketDescriptor, databuf[j], bufsize); // each buffer is size buffsize
			}
		}
		else if(type == 2){
			// single write all the buffers
			int length = nbufs * bufsize;
			write(socketDescriptor, databuf, length);
		}
		else{
            // store the all the segments with the base and length
			for (int j = 0; j < nbufs; j++) {
                vector[j].iov_base = databuf[j];
                vector[j].iov_len = bufsize;
            }

            // write over the segments
            write (socketDescriptor, vector, bufsize);
		}
	}
}

int main(int argc, char** argv) {
    cout << "opened program" << endl;
    char* serverPort = argv[1]; // first argument is the port to the server
	char* serverName = argv[2]; // servers IP address or host name
	int iterations = atoi(argv[3]); // number of iterations a client performs on data transmission using one of the three methods
	int nbufs = atoi(argv[4]); // the number of data buffers
	int bufsize = atoi(argv[5]); // the size of each data buffer (in bytes)
	int type = atoi(argv[6]); // the type of transfer scenario (1 for "multiple writes", 2 for "writev", 3 for "single write")

	// if the type is not between 1 and 3, need to tell user as it is not valid
	if(type < 1 || type > 3){
		cout << "type must be bteween 1 and 3" << endl;
		exit(EXIT_FAILURE);
	}

	// get the linked list of addrinfo that the connection() can understand
	struct addrinfo* addressGuesses = getAddressGuesses(serverPort, serverName);

    cout << "guess 1: " << addressGuesses->ai_addr << endl;

    int sd = getSocketDescriptor(addressGuesses);

    cout << "socket descriptor: " << sd;

    // if the socket descriptor is -1, we couldnt find a successful connection so just kill the program
    if (sd == -1) {
        cout << "Could not find a successful connection" << endl;
        exit (EXIT_FAILURE);
    }

    // need to calculate the transfer time so steps for that are

    // 1. declare needed variables (timeval is just a structure with 2 variables: seconds, and microseconds)
    struct timeval start; // start time of sending data
    struct timeval end; // end total time after sending and reading data
    struct timeval lap; // end time of sending data
    long transferTime; // lap - start
    long totalTime; // end - start

    // 2. start the starttime with the current time of day (i believe this is just linux time)
    gettimeofday(&start, NULL); // sets start tot he current time of day. Second parameter is timezone

    cout << "start time: " << start << endl;

    // 3. Call the function to write to socket
    // if we have found a successful connection, write the data to the socket
    writeToSocket(iterations, nbufs, bufsize, type, sd);

    // 4. When it is done writing, set the value of lap to the current time of day
    gettimeofday(&lap, NULL);

    cout << "time done writing data: " << lap << endl;

    // 5. Now read back the information from the server (takes some time)
    int numReads = 0;
    read(sd, &numReads, sizeof(numReads)); // read takes in a pointer so need to dereference

    // 6. Now check the time after reading (store as end)
    gettimeofday(&end, NULL);

    cout << "time done reading data back from server: " << endl;

    // 7. Calculate the transfer time (lap - start) & the total time (end - start)
    transferTime = ((lap.tv_sec - start.tv_sec) * 1000000L) + (lap.tv_usec - start.tv_usec);
    totalTime = ((end.tv_sec - start.tv_sec) * 1000000L) + (end.tv_usec - start.tv_usec);

    // 8. Print the values in the format from the spec
    cout << "data-transmission time = " << transferTime << " usec, round-trip time = "
        << totalTime << " usec, #reads = " << numReads << endl;

    // 9. Finally, close the socket
    close(sd);

    return 0;
}




/*
 * Notes:
 * structure for the addrinfo (from the netdb class)
 *{
  *int ai_flags;			 Input flags.  
  *int ai_family;		 Protocol family for socket.  
  int ai_socktype;		 Socket type.  
  int ai_protocol;		 Protocol for socket.  
  socklen_t ai_addrlen;		 Length of socket address.  
  struct sockaddr *ai_addr;	 Socket address for socket.  
  char *ai_canonname;		 Canonical name for service location.  
  struct addrinfo *ai_next;	 Pointer to next in list.  
};

 Structure describing a generic socket address. From socket.h
struct sockaddr
  {
    __SOCKADDR_COMMON (sa_);	 Common data: address family and length.  
    char sa_data[14];		 Address data.  
  };

To write to a socket
 a) You can use the write system call with a socket just like how you can with a file and file descriptor
 b) Not sure how this works (TODO: look this up)
 c) The socket descriptor is just like a file descriptor

Buffer
 a) A way of transporting temporary information between two storages
 b) The server needs to handle how that write is dealt with

Socket
 Create a new socket of type TYPE in domain DOMAIN, using
   protocol PROTOCOL.  If PROTOCOL is zero, one is chosen automatically.
   Returns a file descriptor for the new socket, or -1 for errors.  
extern int socket (int __domain, int __type, int __protocol) __THROW;

 */