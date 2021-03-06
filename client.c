/*
 * FTP Client
 * Authors: Janosevic Goran, Kumbeiz Alexander
 * System: Linux Debian x86
 * Date: 2011-01-11
 * Usage: client IP-ADDRESS PORT
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h> 
#include <sys/stat.h>

#define BUF 2048

// State of commands
#define QUIT 0
#define LIST 1
#define GET 2
#define SEND 3
#define BLOCKLIST 4

int main(int argc, char **argv) {

	int sockFd;
	char buffer[BUF];
	struct sockaddr_in address;
	int size;
	long port;
	char *filename = NULL;
	char fn[1024];

	if( argc < 3 ) {
		 fprintf(stderr, "Usage: %s IP-Adresse Port-Nummer\n", argv[0]);
		 exit(EXIT_FAILURE);
	}

	if((sockFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		 perror("Socket error");
		 return EXIT_FAILURE;
	}
  
	port = strtol(argv[2], NULL, 10);

	memset(&address,0,sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	inet_aton(argv[1], &address.sin_addr);

	if(connect( sockFd,(struct sockaddr *) &address, sizeof(address)) == 0) {
		 printf("Connection with server(%s) established\n", inet_ntoa(address.sin_addr));
		 //printf("recv Welcome Message\n");
		 size=recv(sockFd,buffer,BUF-1, 0);
		 
		 if(size > 0) {
			buffer[size]= '\0';
			printf("%s",buffer);
		 }
	} 
	else {
		 perror("Connect error - no server available");
		 return EXIT_FAILURE;
	}
	
	// get username
	printf("Username: ");
	fgets(buffer, BUF-1, stdin);
	if(strlen(buffer) <= 1) {
		strcpy(buffer, "null");
		// send pseudo username and password
		send(sockFd, buffer, strlen(buffer), 0);
		send(sockFd, buffer, strlen(buffer), 0);
		close(sockFd);
		fprintf(stderr, "User invalid\n");
		return EXIT_FAILURE;
	}
	// send username
	send(sockFd, buffer, strlen(buffer), 0);
	
	// get password
	char *pwd;
	pwd=getpass("Password: ");
	strcpy(buffer, pwd);
	if(strlen(buffer) < 1) {
		strcpy(buffer, "null");
		// send pseudo password
		send(sockFd, buffer, strlen(buffer), 0);
		close(sockFd);
		fprintf(stderr, "User or password invalid\n");
		return EXIT_FAILURE;
	}
	// send password
	send(sockFd, buffer, strlen(buffer), 0);

	// recv login message
	size = recv(sockFd, buffer, BUF-1, 0);
	if(size > 0) {
		buffer[size]= '\0';
		printf("%s\n", buffer);
	}

	// status code of command
	int status = -1;

	if(strcmp(buffer, "Username or password invalid\n") == 0) {
		status = QUIT;
	} else if(strcmp(buffer, "User is blocked!\n") == 0) {
		status = QUIT;
	}
	
	while(status != QUIT) {
		
		do {
			// get command
			printf("Send command: ");
			fgets(buffer, BUF-1, stdin);
			
			// get command status
			if(strncmp(buffer, "list", 4) == 0) {
				status = LIST;
			} else if(strncmp(buffer, "get", 3) == 0) {
				status = GET;
				
				// parse filename
				char temp[BUF] = "";
				if(strlen(buffer)-3 > 0)
					// remove 'get'
					strncpy(temp, (buffer+3), strlen(buffer)-3);
				else
					continue;
				filename = strtok(temp, " \n\r");
				// if nothing was after 'get' aks again for command
				if(filename == NULL) {
					status = -1;
					continue;
				}
				strcpy(fn, filename);
			} else if(strncmp(buffer, "send", 4) == 0) {
				status = SEND;
				
				// parse filename
				char temp[BUF] = "";
				if(strlen(buffer)-4 > 0)
					// remove 'send'
					strncpy(temp, (buffer+4), strlen(buffer)-4);
				else
					continue;
				filename = strtok(temp, " \n\r");
				// if nothing was after 'send' aks again for command
				if(filename == NULL) {
					status = -1;
					continue;
				}
				strcpy(fn, filename);
			} else if(strncmp(buffer, "blocklist", 9) == 0) {
				status = BLOCKLIST;
			} else if(strncmp(buffer, "quit", 4) == 0) {
				// quit client and close connection
				status = QUIT;
			} else {
				status = -1;
			}
		} while(status == -1);
		
		// send command
		send(sockFd, buffer, strlen(buffer), 0);
		
		// recv packagesize
		char bufferPackages[BUF];
		size=recv(sockFd,bufferPackages,BUF-1, 0);
		if(size > 0) {
			bufferPackages[size] = '\0';
		}
		
		long packages;
		// convert string to long
		packages = strtol(bufferPackages, NULL, 10);
		if (packages == -1) {
			if (status == GET) {
				printf("File not found!\n");
			}
			continue;
		}
		
		// list
		if (status == LIST) {
			// send confirmation
			send(sockFd,buffer, 1, 0);
			size = 0;
			int i;
			// recv until whole list was received
			for (i=0; i < packages; i+=size) {
				// recv filenames
				size=recv(sockFd, buffer, BUF-1, 0);
				if((size) > 0) {
					// print file list
					buffer[size]= '\0';
					printf("%s",buffer);
					fflush(stdout);
				} else if (size == 0) {
					printf("connect to socket failed\n");
					break;
				} else if (size == -1) {
					printf("error in socket\n");
					continue;
				}
			}
			printf("\n");
		}
		// get
		else if (status == GET) {
			int isConfirmed = -1;
			printf("Size of selected File is %ld bytes\nDo you want to download the file? (y/n)",packages );
			
			// confirm download
			do {
				fgets(buffer, BUF-1, stdin);
				if (strncmp(buffer, "y", 1) == 0) {
					// send confirm yes
					strcpy(buffer, "y");
					send(sockFd, buffer, strlen(buffer), 0);
					isConfirmed = 1;
				} else if (strncmp(buffer, "n", 1) == 0) {
					// send confirm no
					strcpy(buffer, "n");
					send(sockFd, buffer, strlen(buffer), 0);
					isConfirmed = 0;
				} else {
					// try again
					isConfirmed = -1;
					printf("Invalid input. (y/n) ");
				}
			} while(isConfirmed == -1);
			
			// if client sends y
			if(isConfirmed) {
				// get File
				int leftBytes;
				FILE *file = NULL;
				
				// save file in current directory where client was started
				char dirfile[1024] = "./";
				strcat(dirfile, fn);
				
				// progress status
				int progress = 0;
				int percent = 0;
				int diff = 0;
				size = 0;
				
				// open file in write-binary mode
				file = fopen(dirfile, "wb");
				
				// until file is full transmitted
				for(leftBytes = packages; leftBytes > 0; leftBytes -= size) {
					char tempBuffer[BUF] = "";
					// recv part of file an bytes
					size = recv(sockFd, tempBuffer,BUF-1, 0);
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
					
					// Progress bar
					percent = (double)(packages-leftBytes+size)/(double)packages*20;
					diff = percent-progress;
					if(diff >= 0) {
						int i;
						for(i = 0; i < diff; i++) {
							printf("#");
							fflush(stdout);
							progress++;
						}
						if(progress == 20) {
							printf("!\n");
						}
					}
				}
				printf("\n");
				fclose(file);
			}
		// SEND
		} else if (status == SEND) {
			int leftBytes;
			char sendBuffer[BUF];
			char receiveBuffer[BUF];
			FILE *file = NULL;
			struct stat attribut;
			unsigned long sizeOfFile = 0;
			
			// save file in current directory where client was started
			char dirfile[1024] = "./";
			strcat(dirfile, fn);
			
			// get file attributes
			if(stat(dirfile, &attribut) == -1)
			{
				// send -1 as error
				sprintf(sendBuffer, "%ld", (long)-1);
				send(sockFd, sendBuffer, strlen(sendBuffer), 0);
				printf("File not found!\n");
			}
			else {
				// get size of file
				sizeOfFile = attribut.st_size;
				sprintf(sendBuffer, "%ld", sizeOfFile);
				
				// send size of file
				send(sockFd, sendBuffer, strlen(sendBuffer), 0);
				
				// recv confirmation to send file
				size = recv(sockFd, receiveBuffer, 1, 0);
				
				// open file in read-binary mode
				file = fopen(dirfile, "rb");
				if (file != NULL) {
					// size of part of file to send
					int newBUF = BUF-1;
					int size;

					// progress status
					int progress = 0;
					int percent = 0;
					int diff = 0;
					
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
						send(sockFd, tempBuffer, newBUF, 0);
						
						// Progress bar
						percent = (double)(sizeOfFile-leftBytes+newBUF)/(double)sizeOfFile*20;
						diff = percent-progress;
						if(diff >= 0) {
							int i;
							for(i = 0; i < diff; i++) {
								printf("#");
								fflush(stdout);
								progress++;
							}
							if(progress == 20) {
								printf("!\n");
							}
						}
					}
					printf("\n");
					fclose(file);
				} else {
					printf("can't open file\n");
				}
			}
		} else if(status == BLOCKLIST) {
			// send confirmation
			send(sockFd,buffer, 1, 0);
			size = 0;
			int i;
			// recv until whole list was received
			for (i=0; i < packages; i+=size) {
				// recv blocklist
				size=recv(sockFd, buffer, BUF-1, 0);
				if((size) > 0) {
					// print blocklist
					buffer[size]= '\0';
					printf("%s",buffer);
					fflush(stdout);
				} else if (size == 0) {
					printf("connect to socket failed\n");
					break;
				} else if (size == -1) {
					printf("error in socket\n");
					continue;
				}
			}
			printf("\n");
		}
	}

	close(sockFd);
	return EXIT_SUCCESS;
}
