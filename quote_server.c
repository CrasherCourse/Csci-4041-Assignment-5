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

#define SERVER_PORT "6789" /* CHANGE THIS TO THE PORT OF YOUR SERVER */
#define BUFFER_SIZE 1024
#define QUOTE_NAME_SIZE 256
#define QUOTE_FILE_TOTAL 10
#define BACKLOG 10     // how many pending connections queue will hold

char quoteFiles[QUOTE_FILE_TOTAL][QUOTE_NAME_SIZE];
FILE ** inputFiles, *logfile;
int fileCount = 0;

/*********************************************************************
 * get Quote
 **********************************************************************/
void getQuote(char* fileName)
{
	int i;
	char quote[BUFFER_SIZE];
	for(i = 0; i < fileCount; i++)	//determine the file id
	{
		if(strcmp(fileName, quoteFiles[i]) == 0)
		{
			break;
		}
	}
	if(i == fileCount)
	{
		printf("We don't have quotes for %s\n", fileName);
		return;
	}
	fgets(quote, BUFFER_SIZE, inputFiles[i]);
	printf("%s", quote);
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
	printf("Opened: %s\n", config);
	inputFiles = malloc(sizeof(FILE*) * QUOTE_FILE_TOTAL);
	printf("done mallocing\n");
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
void printList(char retval[BUFFER_SIZE])
{
	int i;
	char *temp;
	strcpy(retval, "");
	for(i = 0; i < fileCount; i++)
	{
		sprintf(temp, "%s\n", quoteFiles[i]);
		strcat(retval, temp);
	}
}
// client thread
void clientThread(int * input)
{
	int done = 0;
	int clientSocket = *input;
	printf("Copied the client socket\n");
	char request[BUFFER_SIZE],response[BUFFER_SIZE], temp[BUFFER_SIZE];
	strcpy(response, "this is a test\n");
	while(!done)
	{
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
			printf("Exiting thread\n");
			return;
		}
		if(strcmp(request, "GET: LIST\n") == 0) printList(response);
		else
		{
			printf("Get a quote\n");
		}
		// Send back a response
		if (send(clientSocket, response, sizeof(response),0) < 0){
			printf("Error writing on stream socket");
			perror("Aborting");
			close(clientSocket);
			exit(1);
		}
	}
}
/**********************************************************************
 * main
 **********************************************************************/
/*
int main(int argc, char *argv[])
{
	int i, length, serverSocket;
	struct sockaddr_in servaddr;
	int * clientSocket;
	struct sockaddr_storage clientaddr; // connector's address information
    socklen_t sin_size;

	printf("Start\n");
	if(argc != 2)
	{
		printf("Usage: quote_server config\n");
		exit(0);
	}

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(serverSocket < 0)
	{
		perror("Socket Creation failed");
		exit(0);
	}
	printf("made server socket\n");

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = SERVER_PORT;
	if(bind(serverSocket, (struct sockaddr *)&servaddr, sizeof(servaddr)))
	{
		perror("binding stream socket");
		exit(0);
	}
	printf("Bound Socket!\n");
	if(getsockname(serverSocket, (struct sockaddr *)&servaddr, &length))
	{
		perror("geting socket's port number\n");
		exit(0);
	}
	listen(serverSocket, BACKLOG);
	printf("Ready to serve\n");
	while(1)
	{

	}
	return 0;
}
* */
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
int main(int argc, char** argv)
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

	if(argc != 2)
	{
		printf("Usage: quote_server config\n");
		exit(0);
	}
	makeFileList(argv[1]);

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

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        clientThread(&new_fd);
    }

    return 0;
}


