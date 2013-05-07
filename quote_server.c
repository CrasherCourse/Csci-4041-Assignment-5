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

char quoteFiles[QUOTE_FILE_TOTAL][QUOTE_NAME_SIZE];
pthread_mutex_t logMutex, clientMutex, fileMutex[QUOTE_FILE_TOTAL];
FILE ** inputFiles, *logfile;
int fileCount = 0;

typedef struct clientData{
	int socketID;
} clientData;

/*********************************************************************
 * Code
 **********************************************************************/
// get a Quote
void getQuote(char* fileName, char* retval)
{
	int i;
	char quote[BUFFER_SIZE], temp[BUFFER_SIZE];
	for(i = 0; i < fileCount; i++)	//determine the file id
	{
		if(strncmp(quoteFiles[i], (fileName+16), strlen(quoteFiles[i])) == 0)
		{
			break;
		}
	}
	if(strcmp((fileName+16), "ANY\n") == 0)
	{
		i = (((double)rand())/RAND_MAX * fileCount);		// Generate psuedorandom number
	}
	if(i == fileCount)
	{
		sprintf(retval, "We don't have quotes for %s\n", fileName);
		return;
	}
	pthread_mutex_lock(&fileMutex[i]);		// Protect accessed file
	fgets(quote, BUFFER_SIZE, inputFiles[i]);
	strcpy(retval, quote);
	fgets(quote, BUFFER_SIZE, inputFiles[i]);
	strcat(retval, quote);
	pthread_mutex_unlock(&fileMutex[i]);	// Safe for another thread to read from file
}
// make file list
void makeFileList(char* config)
{
	FILE *fid;
	int index = 0;
	size_t size = BUFFER_SIZE;
	char line[256], *fileName, *save;
	if((fid = fopen(config, "r")) == NULL)
	{
		perror("open: ");
		exit(0);
	}
	if((logfile = fopen("logfile", "w"))==NULL)
	{
		perror("open: ");
		exit(0);
	}
	inputFiles = malloc(sizeof(FILE*) * QUOTE_FILE_TOTAL);
	while(fgets(line, 256, fid) != NULL)
	{
		strcpy(quoteFiles[index],strtok(line, " :\n"));
		fileName = strtok(NULL, " :\n");
		if((inputFiles[index] = fopen(fileName, "r")) == NULL) continue;
		index++;
	}
	fileCount = index;
	fclose(fid);
}
// print list
void printList(char *retval)
{
	int i;
	char *temp;
	strcpy(retval, "");
	for(i = 0; i < fileCount; i++)
	{
		strcat(retval, quoteFiles[i]);
		strcat(retval, "\n");
	}
}
// Record to logefile
record(char * action, char * IP)
{
	char  buffer[BUFFER_SIZE];
	char* datetime;

	int retval;
	time_t  clocktime;
	struct tm  *timeinfo;
	time (&clocktime);
	timeinfo = localtime( &clocktime );
	strftime(buffer, BUFFER_SIZE, "%b-%d-%Y-%H-%M-%S", timeinfo); 
	
	fprintf(logfile, "%s: %s: %s\n", action, buffer, IP);
}
// client handler thread function
void * clientThread(void * input)
{
	int clientSocket = *((int *)input);
	pthread_mutex_unlock(&clientMutex); // safe to allocate new socket
	record("Connection Opened", "IP address");
	char request[BUFFER_SIZE],* response, temp[BUFFER_SIZE];
	response = malloc(sizeof(char)*BUFFER_SIZE);
	while(1)
	{
		strcpy(response, "Invalid command");
		// get a client's request
		if (recv(clientSocket, request, BUFFER_SIZE,0) < 0){
			printf("Error reading from stream socket");
			perror("Aborting");
			close(clientSocket);
			exit(1);
		}
		printf("%s", request);
		if(strcmp(request, "BYE\n") == 0)	//exit
		{
			break;
		}
		else if(strcmp(request, "GET: LIST\n") == 0) printList(response);
		else
		{
			getQuote(request, response);
		}
		// Send back a response
		if (send(clientSocket, response, BUFFER_SIZE,0) < 0){
			printf("Error writing on stream socket");
			perror("Aborting");
			close(clientSocket);
			exit(1);
		}
	}
	record("Connection Closed", "IP address");
	free(response);
	pthread_exit(0);
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
void closeServer(int sig)		// Stops server with ctrl-C
{
	fclose(logfile);
	exit(0);
}
/**********************************************************************
 * main
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
	
	if(argc != 2)
	{
		printf("Usage: quote_server config\n");
		exit(0);
	}
	makeFileList(argv[1]);
	pthread_mutex_init(&logMutex, NULL);
	pthread_mutex_init(&clientMutex, NULL);
	for(i = 0; i < fileCount; i++)
	{
			pthread_mutex_init(&(fileMutex[i]), NULL);
	}
	signal(SIGINT, closeServer);		// Set up server closing protcol
	
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

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

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        pthread_mutex_lock(&clientMutex); // protect transfer of client socket id
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        pthread_create(&tid, NULL, clientThread, ((void *) &new_fd));
    }
    return 0;
}
