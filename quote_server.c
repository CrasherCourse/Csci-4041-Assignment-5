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
#define QUOTE_FILE_TOTAL 20

char quoteFiles[QUOTE_FILE_TOTAL][QUOTE_NAME_SIZE];
FILE ** inputFiles;
int fileCount = 0;
/*********************************************************************
 * get Quote
 **********************************************************************/
void getQuote(char* fileName)
{
	int i;
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
	printf("We have his quotes\n");
}
// make file list
void makeFileList(char* config)
{
	FILE *fid;
	int index = 0;
	size_t size = BUFFER_SIZE;
	char *line;
	if((fid = fopen(config, "r")) == NULL)
	{
		perror("open: ");
		exit(0);
	}
	while(getline(&line, &size, fid) != -1)
	{
		printf("%s", line);
	}
}
// client thread
void * clientThread(void * input)
{
}
/**********************************************************************
 * main
 **********************************************************************/

int main(int argc, char *argv[])
{
	printf("Start\n");
	inputFiles = malloc(sizeof(FILE*) * QUOTE_FILE_TOTAL);
	printf("malloc done\n");
	if(argc != 2)
	{
		printf("Usage: quote_server config\n");
		exit(0);
	}
	makeFileList(argv[1]);
	printf("made list\n");
	getQuote("Einstein");
	getQuote("derpman");
}
