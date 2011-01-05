/* myserver.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>

#define BUF 2048
#define MAXAMOUNT 20

//++++++++++++++++++++
// globale Variablen
//++++++++++++++++++++
char dirname[1024];

//++++++++++++++++++++
// globale Funktionen
//++++++++++++++++++++
void *session(void *arg);
void list(char* dir, int cFd);
FILE* getFile(char*, int);

/*
struct thrdData {
	char* clientAddress;
	short clientPort;
	int clientFd;
	char dirname[1024];
	struct thrdData *nxt;
};
*/

int main(int argc, char **argv) {
	
int sockFd, connFd;
socklen_t addrlen;
struct sockaddr_in address, cliaddress;
long port;


//struct thrdData* data;
pthread_t socketThread[MAXAMOUNT];
int index = 0;

if(argc < 2) {
	fprintf(stderr, "USAGE: %s PORT Directory\n", argv[0]);
	return EXIT_FAILURE;
}

if(argc == 3) {
	strcpy(dirname, argv[2]);
} else {
	getcwd(dirname, sizeof(dirname));
}

port = strtol(argv[1], NULL, 10);
sockFd = socket(AF_INET, SOCK_STREAM, 0);
 
memset(&address,0,sizeof(address));
address.sin_family = AF_INET;
address.sin_addr.s_addr = INADDR_ANY;
address.sin_port = htons(port);

if(bind( sockFd,(struct sockaddr *) &address, sizeof(address)) != 0) {
 perror("bind error");
 return EXIT_FAILURE;
}

listen(sockFd, 5);
addrlen = sizeof(struct sockaddr_in);
	
while(index < MAXAMOUNT){
	printf("Waiting for connections...\n");
	connFd = accept( sockFd,(struct sockaddr *) &cliaddress, &addrlen );
	if(connFd > 0) {
		printf("Client connected from %s:%d...\n", inet_ntoa(cliaddress.sin_addr),ntohs(cliaddress.sin_port));
 /*
	strcpy(buffer,"Welcome to myserver, Please enter your command:\n");
	send(connFd, buffer, strlen(buffer),0);
*/
//		data =(struct thrdData*)malloc(sizeof(struct thrdData));
//		data->clientAddress = inet_ntoa(cliaddress.sin_addr);
//		data->clientPort = ntohs(cliaddress.sin_port);
		pthread_create(&socketThread[index],NULL,session,(void*)&connFd);
		index++;
 } 
 else {
		printf("Fehler bei accept \n");
		return EXIT_FAILURE;
		}
}
	close(sockFd);
	return EXIT_SUCCESS;
}

void *session(void *arg)
{
//	struct thrdData* init =(struct thrdData* )(arg);
	int cFd = *((int *)arg);
	char sendBuffer[BUF];
	char receiveBuffer[BUF];
	int size;
	
	pthread_detach(pthread_self());
	printf("Thread gestartet : %d\n", cFd);
	strcpy(sendBuffer,"Welcome to myserver, Please enter your command:\n");
	send(cFd, sendBuffer, strlen(sendBuffer),0);
	
	while(strncmp(receiveBuffer, "quit", 4) != 0) {
		size = recv(cFd, receiveBuffer, BUF-1, 0);

		if(size > 0) {
			receiveBuffer[size] = '\0';
			printf("Message received: %s\n", receiveBuffer);	
					
			if (strncmp(receiveBuffer, "list", 4) == 0) {
				//strcpy(sendBuffer,"List wurde eingegeben\n");
				//send(cFd, sendBuffer, strlen(sendBuffer),0);
				list(dirname, cFd);
				//dir = opendir(dirname);
				
			} 
			else if (strncmp(receiveBuffer, "get", 3) == 0) {
				char file[1024] = "";
				char *token;
				token = strtok(receiveBuffer, " ");
				token = strtok(NULL, "\n");
				if(token[strlen(token)-1]==13) 
					token[strlen(token)-1]='\0';
				strcat(file, dirname);
				strcat(file, "/");
				strcat(file, token);
				getFile(file, cFd);
				
				
			}
			else {
				strcpy(sendBuffer,"");
			}	
		}
		else if(size == 0) {
			printf("Client closed remote socket\n");
			break;
		}
		else {
			perror("recv error");
			break;
		}
	}
	
	close(cFd);
	return NULL;
}

void list(char *directory, int cFd)
{
	fprintf(stdout, "Fkt gestartet\n");
	struct dirent* dirzeiger = NULL;
	char buffer[BUF];
	DIR *dir;
	dir = opendir(dirname);
	int count = 0;
	while((dirzeiger = readdir(dir))) {
		count++;
	}
	fprintf(stdout, "Counter wurde berechnet\n");
	sprintf(buffer, "%d", count);
	send(cFd, buffer, strlen(buffer), 0);
	closedir(dir);
	fprintf(stdout, "Closed dir\n");
	dir = opendir(dirname);
	while((dirzeiger = readdir(dir))) {
		strcpy(buffer, dirzeiger->d_name);
		strcat(buffer, "\n");
		send(cFd, buffer, strlen(buffer), 0);
	}
	closedir(dir);
	//fflush(stdout);
}

FILE* getFile(char* f, int cFd)
{
	struct stat attribut;
	unsigned long sizeOfFile = 0;
	char choice[1];
	char sendBuffer[BUF];
	
	if(stat(f, &attribut) == -1)
	{
		strcpy(sendBuffer, "Fehler bei stat\n");
		send(cFd, sendBuffer, strlen(sendBuffer), 0);
		return NULL;
	}
	else 
	{
		sizeOfFile = attribut.st_size;
		fprintf(stdout, "Size of selected File %s is %ld bytes\n", f,sizeOfFile );
	}
	
	fprintf(stdout, "Do you want to download the file %s? (y/n)", f);
	fgets(choice, 1, stdin);
	switch(choice[0])
	{
		case 'y':
					fprintf(stdout, "eat this.\n");
					break;
					
		case 'n': 	fprintf(stdout, "that's bad.\n");
					break;
	}
	return NULL;
}