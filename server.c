/*
 * FTP Server
 * Authors: Janosevic Goran, Kumbeiz Alexander
 * System: Linux Debian x86
 * Date: 2011-01-11
 * Usage: server PORT [DIRECTORY]
 */

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
// LDAP constants
#define LDAP_HOST "ldap.technikum-wien.at"
#define SEARCHBASE "dc=technikum-wien,dc=at"
#define SCOPE LDAP_SCOPE_SUBTREE
#define LDAP_PORT 389
// user block restriction
#define BLOCK_DURATION 60*30
#define BLOCK_COUNT 3

// user blocklist entry type
typedef struct ignoreList {
	struct ignoreList *prev;
	char ipAddress[16];
	time_t timeStamp;
	char username[50];
	int count;
	struct ignoreList *next;
} ignoreList;

// Thread data type
typedef struct thrdData {
	char ipAddress[16];
	int socket;
} thrdData;

//++++++++++++++++++++
// global vars
//++++++++++++++++++++
char dirname[1024];
// root of linked blocklist
ignoreList *rootIgnore = NULL;

//++++++++++++++++++++
// global functions
//++++++++++++++++++++
void *session(void *arg);
void list(char* dir, int connFd);
void sendFile(char*, int);
void getFile(char* fn, int connFd);
void blocklist(int connFd);
int verify_user(char *bind_user, char *bind_pw);
void addIgnoreEntry(char *username, char *ipAddress);
int isBlockade(char *username, char *ipAddress);
void cleanIgnoreList();

int main(int argc, char **argv) {

	// connection vars
	int sockFd, connFd;
	socklen_t addrlen;
	struct sockaddr_in address, cliaddress;
	long port;

	// thread vars
	struct thrdData* data;
	pthread_t socketThread;

	if(argc < 2) {
		fprintf(stderr, "USAGE: %s PORT Directory\n", argv[0]);
		return EXIT_FAILURE;
	}

	// Choose directory
	if(argc == 3) {
		strcpy(dirname, argv[2]);
		if(opendir(dirname) == NULL) {
			fprintf(stderr, "%s: %s is not a directory!\n", argv[0], argv[2]);
			return EXIT_FAILURE;
		}
	} else {
		// current dir
		getcwd(dirname, sizeof(dirname));
	}

	// prepare for connections
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

	// wait for clients to connect
	while(1) {
		printf("Waiting for connections...\n");
		connFd = accept( sockFd,(struct sockaddr *) &cliaddress, &addrlen );
		if(connFd > 0) {
			printf("Client connected from %s:%d...\n", inet_ntoa(cliaddress.sin_addr),ntohs(cliaddress.sin_port));
			data =(struct thrdData*)malloc(sizeof(struct thrdData));
			strcpy(data->ipAddress,inet_ntoa(cliaddress.sin_addr));
			data->socket = connFd;
			// create thread for accepted client with session function
			pthread_create(&socketThread,NULL,session,(void*)data);
		 }
		 else {
			printf("Error with accept \n");
			return EXIT_FAILURE;
		}
	}
	close(sockFd);
	return EXIT_SUCCESS;
}

/*
 * Thread and connection session
 */
void *session(void *arg)
{
	thrdData* init =(thrdData* )(arg);
	int connFd = init->socket;
	char sendBuffer[BUF];
	char receiveBuffer[BUF];
	int size;
	char username[BUF];
	char passwd[BUF];

	printf("Running thread: %d\n", connFd);
	
	// send welcome message
	strcpy(sendBuffer,"Welcome to FTP-Server!\n");
	send(connFd, sendBuffer, strlen(sendBuffer),0);

	// recv username
	size = recv(connFd, receiveBuffer, BUF-1, 0);
	if (size > 0) {
		receiveBuffer[size] = '\0';
	}
	strcpy(username, receiveBuffer);
	
	// recv passwd
	size = recv(connFd, receiveBuffer, BUF-1, 0);
	if (size > 0) {
		receiveBuffer[size] = '\0';
	}
	strcpy(passwd, receiveBuffer);
	
	// check user in ldap
	if(verify_user(username, passwd) == 0) {
		// send login error message
		strcpy(sendBuffer, "Username or password invalid\n");
		send(connFd, sendBuffer, strlen(sendBuffer), 0);
		addIgnoreEntry(username, init->ipAddress);
		close(connFd);
		return NULL;
	}
	
	// check blocklist for expired timestamp
	cleanIgnoreList();
	
	// check if user is blocked, otherwise remove entry if exists
	if(isBlockade(username, init->ipAddress) == 1) {
		// send login block message
		strcpy(sendBuffer, "User is blocked!\n");
		send(connFd, sendBuffer, strlen(sendBuffer), 0);
		close(connFd);
		return NULL;
	}
	
	// send login successful message
	send(connFd, "Login successful!\n", BUF-1, 0);
	strcpy(receiveBuffer, "");
	
	// wait for commands from client, runs while command not quit
	while(strncmp(receiveBuffer, "quit", 4) != 0) {
		// recv command
		size = recv(connFd, receiveBuffer, BUF-1, 0);

		if(size > 0) {
			receiveBuffer[size] = '\0';
			
			printf("Message received: %s\n", receiveBuffer);
			
			if (strncmp(receiveBuffer, "list", 4) == 0) {
				// send list
				list(dirname, connFd);
			}
			else if (strncmp(receiveBuffer, "get", 3) == 0) {
				// parse filename
				char *token;
				token = strtok(receiveBuffer, " ");
				token = strtok(NULL, "\n");
				if(token[strlen(token)-1]==13) {
					token[strlen(token)-1]='\0';
				}
				// send file
				sendFile(token, connFd);
			}
			else if(strncmp(receiveBuffer, "send", 4) == 0) {
				// parse filename
				char *token;
				token = strtok(receiveBuffer, " ");
				token = strtok(NULL, "\n");
				if(token[strlen(token)-1]==13) {
					token[strlen(token)-1]='\0';
				}
				// recv file
				getFile(token, connFd);
			}
			else if(strncmp(receiveBuffer, "blocklist", 9) == 0) {
				blocklist(connFd);
			}
			else if(strncmp(receiveBuffer, "quit", 4) == 0) {
				// quit client and connection
				printf("Client closed remote socket\n");
				break;
			} else {
				// clear command buffer
				strcpy(receiveBuffer, "");
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

/*
 * sends sum of filename-sizes
 * and the filenames
 */
void list(char *directory, int connFd)
{
	struct dirent* dirzeiger = NULL;
	struct stat attribut;
	char buffer[BUF] = "";
	DIR *dir;

	long count = 0;
	DIR *isDir;
	
	// parse each element in current directory
	dir = opendir(dirname);
	while((dirzeiger = readdir(dir))) {
		// check if it is file or dir
		isDir = opendir(dirzeiger->d_name);
		if(isDir == NULL)
		{
			// if file, increment size of filename
			if(stat(dirzeiger->d_name, &attribut) == -1) {
				sprintf(buffer, "%s\n", dirzeiger->d_name);
			} else {
				sprintf(buffer, "%s\t%ld B\n", dirzeiger->d_name, attribut.st_size);
			}
			count += strlen(buffer);
		} else {
			// if was dir, close dir
			closedir(isDir);
		}
	}
	closedir(dir);
	
	// send sum of filename-sizes
	sprintf(buffer, "%ld", count);
	send(connFd, buffer, strlen(buffer), 0);
	
	// recv confirmation
	recv(connFd, buffer, BUF-1, 0);

	// open base dir to send each filename
	dir = opendir(dirname);
	while((dirzeiger = readdir(dir))) {
		// only send filenames
		isDir = opendir(dirzeiger->d_name);
		if (isDir == NULL)
		{
			char bufferList[BUF];
			if(stat(dirzeiger->d_name, &attribut) == -1) {
				sprintf(bufferList, "%s\n", dirzeiger->d_name);
			} else {
				sprintf(bufferList, "%s\t%ld B\n", dirzeiger->d_name, attribut.st_size);
			}
			// send filename
			send(connFd, bufferList, strlen(bufferList), 0);
		} else {
			// if it was a dir, close dir
			closedir(isDir);
		}
	}
	closedir(dir);
	fflush(stdout);
}

/*
 * Sends size of file, and file to client.
 */
void sendFile(char* f, int connFd)
{
	char filename[1024] = "";
	struct stat attribut;
	unsigned long sizeOfFile = 0;
	char sendBuffer[BUF];
	char receiveBuffer[BUF];
	FILE *file;
	int leftBytes;
	int size;

	// concatenate dirname and filename
	strcat(filename, dirname);
	strcat(filename, "/");
	strcat(filename, f);

	// get file attributes
	if(stat(filename, &attribut) == -1)
	{
		// send -1 as error
		sprintf(sendBuffer, "%ld", (long)-1);
		send(connFd, sendBuffer, strlen(sendBuffer), 0);
	}
	else
	{
		// get size of file
		sizeOfFile = attribut.st_size;
		sprintf(sendBuffer, "%ld", sizeOfFile);
		
		// send size of file
		send(connFd, sendBuffer, strlen(sendBuffer), 0);
		
		// recv confirmation to send file
		size = recv(connFd, receiveBuffer, BUF-1, 0);
		if (size > 0) {
			receiveBuffer[size] = '\0';
		}
		if(strncmp(receiveBuffer,"y", 1) != 0) {
			return;
		}
		
		// open file in read-binary mode
		file = fopen(filename, "rb");
		if (file != NULL) {
			// size of part of file to send
			int newBUF = BUF-1;
			int size;
			
			// send file parts until whole file is sent
			for(leftBytes = sizeOfFile; leftBytes > 0; leftBytes -= (BUF-1)) {
				char tempBuffer[BUF];
				// set new buffer size for the last part of the file
				if(leftBytes < (BUF-1)) {
					newBUF = leftBytes % (BUF-1);
				}
				
				// read bytes of file
				size = fread(tempBuffer, newBUF, 1, file);
				// send bytes of file
				send(connFd, tempBuffer, newBUF, 0);
			}
			fclose(file);
		} else {
			printf("can't open file\n");
		}
	}
}

/*
 * Loads file from client to server.
 */
void getFile(char* fn, int connFd)
{
	char receiveBuffer[BUF];
	int size;
	// send confirmation (package)
	send(connFd, "0", 1, 0);
	
	// recv size
	size = recv(connFd, receiveBuffer, BUF-1, 0);
	if(size > 0) {
		receiveBuffer[size] = '\0';
	}
	
	long packages;
	// convert string to long
	packages = strtol(receiveBuffer, NULL, 10);
	if (packages == -1) {
		printf("File not found!\n");
		return;
	}
	
	// confirmation, separate package from file
	send(connFd, receiveBuffer, 1, 0);
	
	// get File
	int leftBytes;
	FILE *file = NULL;
	
	// save file in current directory where client was started
	char dirfile[1024] = "./";
	strcat(dirfile, fn);
	
	size = 0;
	
	// open file in write-binary mode
	file = fopen(dirfile, "wb");
	
	// until file is full transmitted
	for(leftBytes = packages; leftBytes > 0; leftBytes -= size) {
		char tempBuffer[BUF] = "";
		// recv part of file an bytes
		size = recv(connFd, tempBuffer,BUF-1, 0);
		if (size > 0) {
			// write bytes in file
			fwrite(tempBuffer, 1, size, file);
		} else if (size == 0) {
			printf("connect to socket failed\n");
			break;
		} else if (size == -1) {
			printf("error in socket\n");
			break;
		}
	}
	printf("\n");
	fclose(file);
}

/*
 * Verify user in LDAP-System. Three-Tier-Check.
 */
int verify_user(char *user, char *pwd)
{
	// Backdoor
	if(strncmp(user, "12", 2) == 0)
		return 1;
		
	// LDAP resource handle
	LDAP *ld;
	LDAPMessage *result, *e;	/* LDAP result handle */
	// Initial bind (Step 1). Set NULL for anonymous bind.
	char bind_user[1024] = "uid=if09b505,ou=People,dc=technikum-wien,dc=at";
	char bind_pwd[1024] = "123";

	// Filter var to search (Step 2).
	char filter[1024] = "";
	char dn[1024] = "";
	// attribute array for search
	char *attribs[3];

	// return value of bind and search
	int rc=0;

	// return uid and cn of entries
	attribs[0]=strdup("uid");
	attribs[1]=strdup("cn");
	// array must be NULL terminated
	attribs[2]=NULL;

	// setup LDAP connection
	if ((ld=ldap_init(LDAP_HOST, LDAP_PORT)) == NULL)    {
		perror("ldap_init failed");
		return 0;
	}

	// STEP 1: anonymous bind
	rc = ldap_simple_bind_s(ld,bind_user,bind_pwd);

	if (rc != LDAP_SUCCESS)    {
	  fprintf(stderr,"LDAP error: %s\n",ldap_err2string(rc));
	  return 0;
	}

	// STEP 2: perform ldap search to given username
	strcpy(filter, "(uid=");
	char *token;
	token = strtok(user, "\n");
	if(token[strlen(token)-1]==13)
		token[strlen(token)-1]='\0';
	strcat(filter, token);
	strcat(filter, ")");

	rc = ldap_search_s(ld, SEARCHBASE, SCOPE, filter, attribs, 0, &result);

	if (rc != LDAP_SUCCESS) {
		fprintf(stderr,"LDAP search error: %s\n",ldap_err2string(rc));
		return 0;
	}

	for (e = ldap_first_entry(ld, result); e != NULL; e = ldap_next_entry(ld,e))
	{
		strcpy(dn, ldap_get_dn(ld,e));
		break;
	}
	// unbind anonymous user or main user
	ldap_unbind(ld);
	
	// setup LDAP connection
	if ((ld=ldap_init(LDAP_HOST, LDAP_PORT)) == NULL)    {
		perror("ldap_init failed");
		return 0;
	}
	
	// STEP 3: bind user to check if pwd is correct
	rc = ldap_simple_bind_s(ld,dn,pwd);

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

/*
 * Add user to block list with ipAddress. 
 * Starts with Counter 1 and current timestamp.
 */
void addIgnoreEntry(char *username, char *ipAddress) {
	ignoreList *temp = rootIgnore;
	
	// search for user, and ipAddress
	while(temp != NULL) {
		if (strcmp(temp->username, username) == 0) {
			if(strcmp(temp->ipAddress, ipAddress) == 0) {
				break;
			}
		}
		temp = temp->next;
	}

	ignoreList *entry = NULL;

	// if not found, add new entry
	if(temp == NULL) {
		entry = (ignoreList*)malloc(sizeof(ignoreList));
		strcpy(entry->username, username);
		entry->timeStamp = time(NULL);
		strcpy(entry->ipAddress, ipAddress);
		entry->prev = NULL;
		entry->next = rootIgnore;
		entry->count = 1;
		rootIgnore = entry;
	}
	// when found, increase counter, and renew timestamp
	else {
		temp->count++;
		temp->timeStamp = time(NULL);
	}
}

/*
 * Check all block entries for expired timestamp.
 * Remove entry of expired timestamp.
 */
void cleanIgnoreList() {
	ignoreList *temp = rootIgnore;
	ignoreList *next = NULL;
	while(temp != NULL) {
		next = temp->next;
		// check if block duration is passed
		if (time(NULL) - temp->timeStamp > BLOCK_DURATION){
			// delete entry
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

/*
 * Check if user is in the block list.
 * When found, check if counter exeeded fail-login limit,
 * else delete entry.
 */
int isBlockade(char *username, char *ipAddress) {
	ignoreList *temp = rootIgnore;
	while(temp != NULL) {
		if (strcmp(temp->username, username) == 0) {
			if(strcmp(temp->ipAddress, ipAddress) == 0) {
				if(temp->count >= BLOCK_COUNT) {
					return 1;
				} else {
					// delete entry
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

/*
 * Sends the list of blocked user.
 */
void blocklist(int connFd) {
	ignoreList *temp = rootIgnore;
	char buffer[BUF];
	int count = 0;
	
	// count bytes to send
	strcpy(buffer, "USERNAME\tIP-ADDRESS\tCOUNTER\n");
	count += strlen(buffer);
	
	while(temp != NULL) {
		sprintf(buffer, "%s\t%s\t%i\n", temp->username, temp->ipAddress, temp->count);
		
		count += strlen(buffer);
		
		temp = temp->next;
	}
	sprintf(buffer, "%i", count);
	// send bytesize
	send(connFd, buffer, strlen(buffer), 0);
	
	// recv confirmation
	recv(connFd, buffer, 1, 0);
	
	temp = rootIgnore;
	
	// send blocklist
	strcpy(buffer, "USERNAME\tIP-ADDRESS\tCOUNTER\n");
	send(connFd, buffer, strlen(buffer), 0);
	
	while(temp != NULL) {
		sprintf(buffer, "%s\t%s\t%i\n", temp->username, temp->ipAddress, temp->count);
		
		send(connFd, buffer, strlen(buffer), 0);
		
		temp = temp->next;
	}
}