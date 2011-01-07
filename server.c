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
#include <errno.h>

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
void list(char* dir, int connFd);
void sendFile(char*, int);
ssize_t writen (int fd, const void *vptr, size_t n);

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
	
while(1){
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
		//index++;
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
	int connFd = *((int *)arg);
	char sendBuffer[BUF];
	char receiveBuffer[BUF];
	int size;
	
	//pthread_detach(pthread_self());
	printf("Thread gestartet : %d\n", connFd);
	strcpy(sendBuffer,"Welcome to myserver, Please enter your command:\n");
	send(connFd, sendBuffer, strlen(sendBuffer),0);
	
	while(strncmp(receiveBuffer, "quit", 4) != 0) {
		size = recv(connFd, receiveBuffer, BUF-1, 0);

		if(size > 0) {
			receiveBuffer[size] = '\0';
			printf("Message received: %s\n", receiveBuffer);	
			if (strncmp(receiveBuffer, "list", 4) == 0) {
				//strcpy(sendBuffer,"List wurde eingegeben\n");
				//send(connFd, sendBuffer, strlen(sendBuffer),0);
				list(dirname, connFd);
				//dir = opendir(dirname);
				
			} 
			else if (strncmp(receiveBuffer, "get", 3) == 0) {
				char *token;
				token = strtok(receiveBuffer, " ");
				token = strtok(NULL, "\n");
				if(token[strlen(token)-1]==13) 
					token[strlen(token)-1]='\0';
				sendFile(token, connFd);
			}
			else if(strncmp(receiveBuffer, "quit", 4) == 0) {
				printf("Client closed remote socket\n");
				break;
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
	
	close(connFd);
	return NULL;
}

void list(char *directory, int connFd)
{
	struct dirent* dirzeiger = NULL;
	char buffer[BUF];
	DIR *dir;
	dir = opendir(dirname);
	int count = 0;
	while((dirzeiger = readdir(dir))) {
		count++;
	}
	sprintf(buffer, "%d", count);
	send(connFd, buffer, strlen(buffer), 0);
	closedir(dir);
	dir = opendir(dirname);
	while((dirzeiger = readdir(dir))) {
		strcpy(buffer, dirzeiger->d_name);
		strcat(buffer, "\n");
		send(connFd, buffer, strlen(buffer), 0);
	}
	closedir(dir);
	//fflush(stdout);
}

void sendFile(char* f, int connFd)
{
	char filename[1024] = "";
	struct stat attribut;
	unsigned long sizeOfFile = 0;
	char sendBuffer[BUF];
	FILE *file;
	int leftBytes;
	
	strcat(filename, dirname);
	strcat(filename, "/");
	strcat(filename, f);
	
	if(stat(filename, &attribut) == -1)
	{
		sprintf(sendBuffer, "%d", -1);
		send(connFd, sendBuffer, strlen(sendBuffer), 0);
	}
	else 
	{
		sizeOfFile = attribut.st_size;
		sprintf(sendBuffer, "%ld", sizeOfFile);
		writen(connFd, sendBuffer, BUF-1);
		recv(connFd, sendBuffer, BUF-1, 0);
		if(strncmp(sendBuffer,"y", 1) != 0) {
			return;
		}
		file = fopen(filename, "rb");
		if (file != NULL) {
			int newBUF = BUF-1;
			char tempBuffer[BUF] = "";
			int i = 0;
			for(leftBytes = sizeOfFile; leftBytes >= 0; leftBytes -= (BUF-1)) {
				if(leftBytes < (BUF-1)) {
					newBUF = leftBytes % (BUF-1);
				}
				fread(tempBuffer, newBUF, 1, file);
				send(connFd, tempBuffer, newBUF, 0);
				printf("newBuf %i leftByte %i size %ld\n", newBUF, leftBytes, sizeOfFile);
			}
			i++;
		}
		fclose(file);
	}
}

// write n bytes to a descriptor ...
ssize_t writen (int fd, const void *vptr, size_t n)
{
	size_t nleft ;
	ssize_t nwritten ;
	const char *ptr ;
	ptr = vptr ;
	nleft = n ;
	while (nleft > 0) {
		if ( (nwritten = write(fd,ptr,nleft)) <= 0) {
			if (errno == EINTR)
				nwritten = 0 ; // and call write() again
			else
			return (-1) ;
		}
		nleft -= nwritten ;
		ptr += nwritten ;
	} ;
	return (n) ;
} ;