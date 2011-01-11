/*
 * Authoren: Janosevic Goran, Kumbeiz Alexander
 * 
 *
 */

/* myclient.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h> 

#define BUF 2048
ssize_t readline (int fd, void *vptr, size_t maxlen);
static ssize_t my_read (int fd, char *ptr);

int main(int argc, char **argv) {

	int sockFd;
	char buffer[BUF];
	struct sockaddr_in address;
	int size;
	long port;
	char *filename = NULL;
	char fn[1024];

	if( argc < 2 ) {
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
	
	printf("Username: ");
	fgets(buffer, BUF-1, stdin);
	//printf("send username\n");
	send(sockFd, buffer, strlen(buffer), 0);
	
	char *pwd;
	pwd=getpass("Password: ");
	strcpy(buffer, pwd);
	//printf("send password\n");
	send(sockFd, buffer, strlen(buffer), 0);
	//printf("recv loginmessage\n");
	size = recv(sockFd, buffer, BUF-1, 0);
	if(size > 0) {
		buffer[size]= '\0';
		printf("%s\n", buffer);
	}

	int status = -1;
	
	if(strcmp(buffer, "Username or password invalid\n") == 0) {
		status = 0;
	} else if(strcmp(buffer, "User is blocked!\n") == 0) {
		status = 0;
	}
	
	while(status != 0) {
		
		do {
			printf("Send command: ");
			fgets(buffer, BUF-1, stdin);
			
			if(strncmp(buffer, "list", 4) == 0) {
				status = 1;
			} else if(strncmp(buffer, "get", 3) == 0) {
				status = 2;
				char temp[BUF] = "";
				strcpy(temp, buffer);
				filename = strtok(temp, " ");
				filename = strtok(NULL, "\n");
				if(filename[strlen(filename)-1]==13) 
					filename[strlen(filename)-1]='\0';
				strcpy(fn, filename);
			} else if(strncmp(buffer, "quit", 4) == 0) {
				status = 0;
			} else {
				status = -1;
			}
		} while(status == -1);
		//printf("send command\n");
		send(sockFd, buffer, strlen(buffer), 0);
		//printf("recv packages\n");
		char bufferPackages[BUF];
		size=recv(sockFd,bufferPackages,BUF-1, 0);
		if(size > 0) {
			bufferPackages[size] = '\0';
		}
		long packages;
		packages = strtol(bufferPackages, NULL, 10);
		//printf("packages %s,%ld\n", bufferPackages, packages);
		if (packages == -1) {
			if (status == 2) {
				printf("File not found!\n");
			}
			continue;
		}
		// list
		if (status == 1) {
			int i;
			char bufferList[BUF] = "";
			for (i=0; i < packages; i++) {
				//printf("recv list\n");
				//strcpy(bufferList, "");
				size=readline(sockFd, bufferList, BUF-1);
				if((size) > 0) {
					//buffer[size]= '\0';
					printf("%s",bufferList);
					fflush(stdout);
					//continue;
				} else if (size == 0) {
					printf("connect to socket failed\n");
					break;
				} else if (size == -1) {
					printf("error in socket\n");
					break;
				}
			}
		}
		// get
		else if (status == 2) {
			int isConfirmed = -1;
			//size=recv(sockFd, buffer, BUF-1, 0);
			//printf("%s ", buffer);
			printf("Size of selected File is %ld bytes\nDo you want to download the file? (y/n)",packages );
			do {
				fgets(buffer, BUF-1, stdin);
				if (strncmp(buffer, "y", 1) == 0) {
					//printf("send confirm download y\n");
					strcpy(buffer, "y");
					send(sockFd, buffer, strlen(buffer), 0);
					isConfirmed = 1;
				} else if (strncmp(buffer, "n", 1) == 0) {
					//printf("send confirm download n\n");
					strcpy(buffer, "n");
					send(sockFd, buffer, strlen(buffer), 0);
					isConfirmed = 0;
				} else {
					isConfirmed = -1;
					printf("Invalid input. (y/n) ");
				}
			} while(isConfirmed == -1);
			if(isConfirmed) {
				// get File
				int leftBytes;
				FILE *file = NULL;
				char dirfile[1024] = "./";
				strcat(dirfile, fn);
				file = fopen(dirfile, "wb");
				int progress = 0;
				int percent = 0;
				int j = 1;
				int newBUF = BUF-1;
				size = 0;
				for(leftBytes = packages; leftBytes > 0; leftBytes -= size) {
					char tempBuffer[BUF] = "";
					/*if(leftBytes < (BUF-1)) {
						newBUF = leftBytes % (BUF-1);
					}*/
					percent = (double)(packages-leftBytes+newBUF)/(double)packages*20;
					int temp = percent-progress;
					if(temp >= 0) {
						//printf("percent: %i progress: %i diff: %i\n", percent, progress, temp);
						int i;
						for(i = 0; i < temp; i++) {
							printf("#");
							fflush(stdout);
							progress++;
						}
						if(progress == 20) {
							printf("!\n");
						}
					}
					//printf("recv file\n");
					size = recv(sockFd, tempBuffer,BUF-1, 0);
					if (size > 0) {
						//printf("%i.recvfile\n", j);
						//tempBuffer[size] = '\0';
					} else if (size == 0) {
						printf("connect to socket failed\n");
						break;
					} else if (size == -1) {
						printf("error in socket\n");
						break;
					}
					fwrite(tempBuffer, 1, size, file);
					j++;
				}
				fclose(file);
			}
		}
	}

	close(sockFd);
	return EXIT_SUCCESS;
}


static ssize_t
my_read (int fd, char *ptr)
{
	static int read_cnt = 0 ;
	static char *read_ptr ;
	static char read_buf[BUF] ;
	if (read_cnt <= 0) {
		again:
		if ( (read_cnt = read(fd,read_buf,sizeof(read_buf))) < 0) {
			if (errno == EINTR)
				goto again ;
			return (-1) ;
		} else if (read_cnt == 0)
			return (0) ;
		read_ptr = read_buf ;
	} ;
	read_cnt-- ;
	*ptr = *read_ptr++ ;
	return (1) ;
}

ssize_t readline (int fd, void *vptr, size_t maxlen)
{
	ssize_t n, rc ;
	char c, *ptr ;
	ptr = vptr ;
	for (n = 1 ; n < maxlen ; n++) {
		if ( (rc = my_read(fd,&c)) == 1 ) {
			*ptr++ = c ;
			if (c == '\n')
				break ; // newline is stored
		} else if (rc == 0) {
			if (n == 1)
				return (0) ; // EOF, no data read
			else
				break ; // EOF, some data was read
		} else
			return (-1) ; // error, errno set by read() in my_read()
	} ;
	*ptr = 0 ; // null terminate
	return (n) ;
}
