/* myclient.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
 
#define BUF 1024

int main(int argc, char **argv) {

int create_socket;
char buffer[BUF];
struct sockaddr_in address;
int size, check = 0;
long port;

if( argc < 2 ) {
     fprintf(stderr, "Usage: %s IP-Adresse Port-Nummer\n", argv[0]);
     exit(EXIT_FAILURE);
}

if((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
     perror("Socket error");
     return EXIT_FAILURE;
}
  
  port = strtol(argv[2], NULL, 10);
  
  memset(&address,0,sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  inet_aton(argv[1], &address.sin_addr);

if(connect( create_socket,(struct sockaddr *) &address, sizeof(address)) == 0) {
     printf("Connection with server(%s) established\n", inet_ntoa(address.sin_addr));
     size=recv(create_socket,buffer,BUF-1, 0);
     
     if(size > 0) {
        buffer[size]= '\0';
        printf("%s",buffer);
     }
} 
else {
     perror("Connect error - no server available");
     return EXIT_FAILURE;
}

while(strncmp(buffer, "quit", 4) != 0) { 
	
	printf("Send command: ");
	fgets(buffer, BUF, stdin);
	
	if((strncmp(buffer, "list", 4) != 0) && (strncmp(buffer, "get", 3) != 0) && (strncmp(buffer, "quit", 4) != 0)) {
		printf("passt nicht\n");
		while(check == 0) {
			printf("Send correct command (quit, get, list): ");
			fgets(buffer, BUF, stdin);
			
			if(strncmp(buffer, "list", 4) == 0) {
				check = 1;
				break;
			}

			if(strncmp(buffer, "get", 3) == 0) {
				check = 1;
				break;
			}
			
			if(strncmp(buffer, "quit", 4) == 0) {
				check = 1;
				break;
			}
		}
		check = 0;
	}
		send(create_socket, buffer, strlen(buffer), 0);
		size=recv(create_socket,buffer,BUF-1, 0);

		if((size) > 0) {
			buffer[size]= '\0';
			printf("%s",buffer);
			//continue;
		}
}
 
  close(create_socket);
  return EXIT_SUCCESS;
}
