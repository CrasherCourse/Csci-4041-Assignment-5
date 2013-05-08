/**********************************************************************/
/* Requisite includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>

#define SERVER_PORT "6789" /* CHANGE THIS TO THE PORT OF YOUR SERVER */
#define BUFFER_SIZE 1024
#define QUOTE_NAME_SIZE 256
#define QUOTE_FILE_TOTAL 10
#define BACKLOG 10     // how many pending connections queue will hold

char config[BUFFER_SIZE], quoteNames[QUOTE_FILE_TOTAL][QUOTE_NAME_SIZE];
int madeListOnce = 0;
pthread_mutex_t logMutex, clientMutex;
FILE *logfile;

// clientData is used to transfer info about clients to threads
typedef struct clientData{
	int socketID;
	char clientIP[BUFFER_SIZE];
} clientData;

/*********************************************************************
 * Code
 **********************************************************************/
// get a Quote
// Trys to find matching file, gets random one for "ANY"
void getQuote(char * selector, FILE** quoteFiles, int quoteCount, char* retval)
{
	int i;
	char quote[BUFFER_SIZE], temp[BUFFER_SIZE];
	printf("%s\n", (selector+16));
	for(i = 0; i < quoteCount; i++)				//determine the file id
	{
		printf("%s\n", quoteNames[i]);
		if(strncmp(quoteNames[i], (selector+16), strlen(quoteNames[i])) == 0)
		{
			break;
		}
	}
	if(strcmp((selector+16), "ANY\n") == 0)					// Check if client requested for ANY
	{
		i = (((double)rand())/RAND_MAX * quoteCount);		// Generate psuedorandom number
	}
	if(i == quoteCount)										// No file matches name, return appropriate response
	{
		sprintf(retval, "We don't have quotes for %s\n", selector);
		return;
	}
	if(fgets(quote, BUFFER_SIZE, quoteFiles[i]) == 0){		// Get quote
		rewind(quoteFiles[i]);								// go back to the beginning if at end
		fgets(quote, BUFFER_SIZE, quoteFiles[i]);
	}
	strcpy(retval, quote);									// Save the quote
	fgets(quote, BUFFER_SIZE, quoteFiles[i]);				// Get name of quote's speaker
	strcat(retval, quote);									// put at end of retval
}

// Make file list
// Also gets a list of Names and the quote file total
void makeFileList(FILE ** quoteFiles, int *quoteCount)
{
	FILE *fid;
	int index = 0;
	size_t size = BUFFER_SIZE;
	char line[256], *temp, *save;
	
	if((fid = fopen(config, "r")) == NULL)	// open config file
	{
		perror("open: ");
		exit(0);							// if config is unopenable, it will never work
	}

	while(fgets(line, 256, fid) != NULL)					// get names of each file from config
	{
		if(!madeListOnce) strcpy(quoteNames[index], strtok(line, " :\n"));		// save the file name, once
		else strtok(line, " :\n");	// just discard first part
		temp = strtok(NULL, " :\n");
		if((quoteFiles[index] = fopen(temp, "r")) == NULL) continue;	// open quote file
		index++;
	}
	*quoteCount = index;	// save total number of quote files
	fclose(fid);		// done with the config file
}

// printList
// print list of quotes to retval parameter
void printList(char *retval, int quoteCount)
{
	int i;
	char *temp;
	strcpy(retval, "");
	for(i = 0; i < quoteCount; i++)
	{
		strcat(retval, quoteNames[i]);
		strcat(retval, "\n");
	}
}

// record
// Record to logefile the action: time: IP
record(char * action, char * IP)
{
	char  buffer[BUFFER_SIZE];
	char* datetime;

	int retval;
	time_t  clocktime;
	struct tm  *timeinfo;
	time (&clocktime);
	timeinfo = localtime( &clocktime );									// get local time
	strftime(buffer, BUFFER_SIZE, "%b-%d-%Y-%H-%M-%S", timeinfo); 		// make the time string
	pthread_mutex_lock(&logMutex);										// protect the logfile
	fprintf(logfile, "%s: %s: %s\n", action, buffer, IP);				// print to log
	pthread_mutex_unlock(&logMutex);
}

// clientThread
// client handler thread function
void * clientThread(void * input)
{
	int clientSocket = ((clientData *)input)->socketID;		// save socket fp
	char request[BUFFER_SIZE],* response, IP[BUFFER_SIZE];
	strcpy(IP, ((clientData *)input)->clientIP);			// copy client IP
	pthread_mutex_unlock(&clientMutex); 					// safe to allocate new socket
	record("Connection Opened", IP);						// write to logfile
	int quoteCount, i;											// Total number of files
	FILE ** quoteFiles = malloc(QUOTE_FILE_TOTAL * (sizeof(FILE*)));	// list of quote file pointers
	response = malloc(sizeof(char)*BUFFER_SIZE);	// allocate size for response
	
	makeFileList(quoteFiles, &quoteCount);
	while(1)
	{
		// get a client's request
		if (recv(clientSocket, request, BUFFER_SIZE,0) < 0){
			printf("Error reading from stream socket");
			perror("Aborting");
			close(clientSocket);
			exit(1);
		}
		if(strcmp(request, "BYE\n") == 0)										// exit client thread
		{
			break;
		}
		else if(strcmp(request, "GET: LIST\n") == 0) printList(response, quoteCount);	// give client a list of quotes
		else
		{
			getQuote(request, quoteFiles, quoteCount, response);	// get a quote
		}
		// Send back a response
		if (send(clientSocket, response, BUFFER_SIZE,0) < 0){
			printf("Error writing on stream socket");
			perror("Aborting");
			close(clientSocket);
			exit(1);
		}
	}
	record("Connection Closed", IP);	// Record closing client thread
	free(response);						// Free up malloced files
	free(quoteFiles);
	pthread_exit(0);					// Client thread is done
}

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// closeServer
// Stops server with ctrl-C, makes sure logfile is properly closed
void closeServer(int sig)
{
	fclose(logfile);
	exit(0);
}
/**********************************************************************
 * Main
 **********************************************************************/
int main(int argc, char** argv)
{
    int sockfd, new_fd, i;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    pthread_t tid;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    clientData cd;
	
	if(argc != 2)		// make sure arguments are in order
	{
		printf("Usage: quote_server config\n");
		exit(0);
	}
	strcpy(config, argv[1]);	// copy name of config file
	
	// Set up mutexes used by program
	pthread_mutex_init(&logMutex, NULL);
	pthread_mutex_init(&clientMutex, NULL);
	signal(SIGINT, closeServer);		// Set up server closing protcol
	
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; 		// use my IP

    if ((rv = getaddrinfo(NULL, SERVER_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }
    freeaddrinfo(servinfo); // all done with this structure
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
	if((logfile = fopen("logfile", "w"))==NULL)	// open logfile for book keeping
	{
		perror("open: ");
		exit(0);
	}
	
    while(1) {  											// main accept() loop
        sin_size = sizeof their_addr;
        pthread_mutex_lock(&clientMutex); 					// protect transfer of client socket id
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);		// accept a client
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        // put data into a packet and ship it to a client thread
        cd.socketID = new_fd;
        inet_ntop(AF_INET, (struct sockaddr *)&their_addr, cd.clientIP, sizeof cd.clientIP);
        pthread_create(&tid, NULL, clientThread, ((void *) &cd));	// create thread for new client
    }
    return 0;
}
