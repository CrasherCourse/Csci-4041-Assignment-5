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

#define SERVER_PORT 6789 /* CHANGE THIS TO THE PORT OF YOUR SERVER */
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
void printList(void)
{
	int i;
	for(i = 0; i < fileCount; i++)
	{
		printf("%s\n", quoteFiles[i]);
	}
}
// client thread
void * clientThread(int * input)
{
	int done = 0;
	int clientSocket = *input;
	printf("Copied the client socket\n");
	char request[BUFFER_SIZE],response[BUFFER_SIZE];
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
		printf("%s\n", request);
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

int main(int argc, char *argv[])
{
	int i, length, serverSocket;
	struct sockaddr_in servaddr;
	int * clientSocket;

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
	listen(serverSocket, BACKLOG);
	printf("Ready to serve\n");
	while(1)
	{
		break;
	}
	return 0;
}
