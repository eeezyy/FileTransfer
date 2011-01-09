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
#include <ldap.h>
#include <time.h>

#define BUF 2048
#define MAXAMOUNT 20
#define LDAP_HOST "ldap.technikum-wien.at"
#define LDAP_PORT 389
#define BLOCK_DURATION 60*30
#define BLOCK_COUNT 3

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
int verify_user(char *bind_user, char *bind_pw);
void addIgnoreEntry(char *username, char *ipAddress);
int isBlockade(char *username, char *ipAddress);
void cleanIgnoreList();
//ssize_t writen (int fd, const void *vptr, size_t n);

typedef struct ignoreList {
	struct ignoreList *prev;
	char ipAddress[16];
	time_t timeStamp;
	char username[50];
	int count;
	struct ignoreList *next;
} ignoreList;

typedef struct thrdData {
	char ipAddress[16];
	int socket;
} thrdData;

ignoreList *rootIgnore = NULL;

int main(int argc, char **argv) {
	
int sockFd, connFd;
socklen_t addrlen;
struct sockaddr_in address, cliaddress;
long port;

struct thrdData* data;
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
		data =(struct thrdData*)malloc(sizeof(struct thrdData));
		strcpy(data->ipAddress,inet_ntoa(cliaddress.sin_addr));
//		data->clientPort = ntohs(cliaddress.sin_port);
		data->socket = connFd;
		pthread_create(&socketThread[index],NULL,session,(void*)data);
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
	thrdData* init =(thrdData* )(arg);
	int connFd = init->socket;
	char sendBuffer[BUF];
	char receiveBuffer[BUF];
	int size;
	char username[BUF];
	char passwd[BUF];
	
	//pthread_detach(pthread_self());
	printf("Thread gestartet : %d\n", connFd);
	strcpy(sendBuffer,"Welcome to myserver, Please enter your command:\n");
	//printf("send welcome message\n");
	send(connFd, sendBuffer, BUF-1,0);
	
	// recv username
	//printf("recv username\n");
	recv(connFd, receiveBuffer, BUF-1, 0);
	strcpy(username, receiveBuffer);
	// recv passwd
	//printf("recv password\n");
	recv(connFd, receiveBuffer, BUF-1, 0);
	strcpy(passwd, receiveBuffer);
	if(verify_user(username, passwd) == 0) {
		//printf("send loginmessage invalid\n");
		send(connFd, "Username or password invalid\n", BUF-1, 0);
		addIgnoreEntry(username, init->ipAddress);
		close(connFd);
		return NULL;
	}
	cleanIgnoreList();
	if(isBlockade(username, init->ipAddress) == 1) {
		//printf("send loginmessage blocked\n");
		send(connFd, "User is blocked!\n", BUF-1, 0);
		close(connFd);
		return NULL;
	}
	//printf("send loginmessage successful\n");
	send(connFd, "Login successful!\n", BUF-1, 0);
	strcpy(receiveBuffer, "");
	while(strncmp(receiveBuffer, "quit", 4) != 0) {
		//printf("recv command\n");
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
	long count = 0;
	DIR *temp;
	while((dirzeiger = readdir(dir))) {
		temp = opendir(dirzeiger->d_name);
		if(temp == NULL)
		{
			count++;
		} else {
			closedir(temp);
		}
	}
	strcpy(buffer, "");
	sprintf(buffer, "%ld", count);
	//printf("send packages\n");
	send(connFd, buffer, BUF-1, 0);
	closedir(dir);
	dir = opendir(dirname);
	
	while((dirzeiger = readdir(dir))) {
		temp = opendir(dirzeiger->d_name);
		if (temp == NULL)
		{
			char bufferList[BUF];
			//printf("%s\n", dirzeiger->d_name);
			strcpy(bufferList, "");
			strcpy(bufferList, dirzeiger->d_name);
			strcat(bufferList, "\n");
			//printf("send list\n");
			send(connFd, bufferList, strlen(bufferList), 0);
		} else {
			closedir(temp);
		}
	}
	closedir(dir);
	fflush(stdout);
}

void sendFile(char* f, int connFd)
{
	char filename[1024] = "";
	struct stat attribut;
	unsigned long sizeOfFile = 0;
	char sendBuffer[BUF];
	FILE *file;
	int leftBytes;
	//int iterations = 1;
	//int i;
	
	//parsing given filename => wildcards
	/*for(i = 0; i < strlen(f); i++) {
		if( f[i] == '*') {
			//calling function which returns a matrix (array[#files][0])
			printf("\nSchternsche g'funde!\n");
			break;
		}
	}*/
	
	strcat(filename, dirname);
	strcat(filename, "/");
	strcat(filename, f);
	
	if(stat(filename, &attribut) == -1)
	{
		sprintf(sendBuffer, "%ld", (long)-1);
		//printf("send packages stat error\n");
		send(connFd, sendBuffer, BUF-1, 0);
	}
	else 
	{
		//for-loop for implementation of wildcards acceptance
		//for(i = 0; i < iterations; i++) 
		{
		sizeOfFile = attribut.st_size;
		sprintf(sendBuffer, "%ld", sizeOfFile);
		//printf("send packages\n");
		send(connFd, sendBuffer, BUF-1, 0);
		//printf("recv confirm download\n");
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
				//printf("send file\n");
				send(connFd, tempBuffer, newBUF, 0);
				//printf("newBuf %i leftByte %i size %ld\n", newBUF, leftBytes, sizeOfFile);
			}
			i++;
		}
		fclose(file);
		}
	}
}

int verify_user(char *user, char *bind_pw)
{
   LDAP *ld;			/* LDAP resource handle */
	char bind_user[1024] = "";
   int rc=0;

   /* setup LDAP connection */
   if ((ld=ldap_init(LDAP_HOST, LDAP_PORT)) == NULL)
   {
      perror("ldap_init failed");
      return 0;
   }

   //printf("connected to LDAP server %s on port %d\n",LDAP_HOST,LDAP_PORT);
	char *token;
	token = strtok(user, "\n");
	if(token[strlen(token)-1]==13) 
		token[strlen(token)-1]='\0';
		
	char *pw_token;
	pw_token = strtok(bind_pw, "\n");
	if(pw_token[strlen(pw_token)-1] == 13)
		pw_token[strlen(pw_token)-1]='\0';
   sprintf(bind_user, "uid=%s,ou=People,dc=technikum-wien,dc=at", token);
   rc = ldap_simple_bind_s(ld,bind_user,bind_pw);

   if (rc != LDAP_SUCCESS)
   {
		perror("ldap_bind failed");
		ldap_unbind(ld);
		return 0;
   }
   else
   {
		printf("login!\n");
		ldap_unbind(ld);
		return 1;
   }
}

void addIgnoreEntry(char *username, char *ipAddress) {
	ignoreList *temp = rootIgnore;
	while(temp != NULL) {
		if (strcmp(temp->username, username) == 0) {
			if(strcmp(temp->ipAddress, ipAddress) == 0) {
				break;
			}
		}
		temp = temp->next;
	}
	
	ignoreList *entry = NULL;
	
	if(temp == NULL) {
		entry = (ignoreList*)malloc(sizeof(ignoreList));
		strcpy(entry->username, username);
		entry->timeStamp = time(NULL);
		strcpy(entry->ipAddress, ipAddress);
		entry->prev = NULL;
		entry->next = rootIgnore;
		entry->count = 1;
		rootIgnore = entry;
	} else {
		temp->count++;
		temp->timeStamp = time(NULL);
	}
}

void cleanIgnoreList() {
	ignoreList *temp = rootIgnore;
	ignoreList *next = NULL;
	while(temp != NULL) {
		next = temp->next;
		if (time(NULL) - temp->timeStamp > BLOCK_DURATION){
			if(temp->prev != NULL) {
				temp->prev->next = temp->next;
			} else {
				rootIgnore = temp->next;
			}
			if(temp->next != NULL) {
				temp->next->prev = temp->prev;
			}
			free(temp);
		}
		temp = next;
	}
}

int isBlockade(char *username, char *ipAddress) {
	ignoreList *temp = rootIgnore;
	while(temp != NULL) {
		if (strcmp(temp->username, username) == 0) {
			if(strcmp(temp->ipAddress, ipAddress) == 0) {
				if(temp->count >= BLOCK_COUNT) {
					return 1;
				} else {
					if(temp->prev != NULL) {
						temp->prev->next = temp->next;
					} else {
						rootIgnore = temp->next;
					}
					if(temp->next != NULL) {
						temp->next->prev = temp->prev;
					}
					free(temp);
					return 0;
				}
			}
		}
		temp = temp->next;
	}
	return 0;
}

// write n bytes to a descriptor ...
/*ssize_t writen (int fd, const void *vptr, size_t n)
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
} ;*/
