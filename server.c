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
#include <sys/stat.h>
#define BUF 1024

FILE* getFile(char*);
void listChar(char*);

int main (int argc, char **argv) {
  int create_socket, new_socket;
  socklen_t addrlen;
  char buffer[BUF];
  int size;
  struct sockaddr_in address, cliaddress;
  char dirname[1024];
  char *token;

  if(argc < 2) {
	  fprintf(stderr, "USAGE: %s PORT [Directory]\n", argv[0]);
	  return EXIT_FAILURE;
  }
  if(argc == 3) {
	strcpy(dirname, argv[2]);
  } else {
	getcwd(dirname, sizeof(dirname));
  }

  // TODO: Überprüfen, ob Verzeichnis geöffnet werden kann, ansonsten fehler
  DIR *dir = opendir(dirname);

  create_socket = socket (AF_INET, SOCK_STREAM, 0);

  memset(&address,0,sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons (strtol(argv[1], NULL, 10));

  if (bind ( create_socket, (struct sockaddr *) &address, sizeof (address)) != 0) {
     perror("bind error");
     return EXIT_FAILURE;
  }
  listen (create_socket, 5);
  
  addrlen = sizeof (struct sockaddr_in);

  //while (1) 
  {
     printf("Waiting for connections...\n");
     new_socket = accept ( create_socket, (struct sockaddr *) &cliaddress, &addrlen );
     if (new_socket > 0)
     {
        printf ("Client connected from %s:%d...\n", inet_ntoa (cliaddress.sin_addr),ntohs(cliaddress.sin_port));
        strcpy(buffer,"Welcome to ftp-server, please enter your command:\n");
        send(new_socket, buffer, strlen(buffer),0);
     }
     do {
        size = recv (new_socket, buffer, BUF-1, 0);
		
        if( size > 0)
        {
			if (strncmp(buffer, "list", 4) == 0) {
				// send file list
				struct dirent* dirzeiger;
				while((dirzeiger = readdir(dir)) != NULL) {
					printf("%s\n", dirzeiger->d_name);
				}
				//fflush(stdout);


			} else if (strncmp(buffer, "get", 3) == 0) {	
				// select needed file from input
				char file[1024] = "";
				
				token = strtok(buffer, " ");
				token = strtok(NULL, "\n");
				if(token[strlen(token)-1]==13) 
					token[strlen(token)-1]='\0';
				strcat(file, dirname);
				strcat(file, "/");
				strcat(file, token);
				getFile(file);
			} else if (strncmp (buffer, "quit", 4)  == 0) {
				// close server
				break;
			}
			buffer[size] = '\0'; //+
           printf ("Message received: %s\n", buffer);
        }
        else if (size == 0)
        {
           printf("Client closed remote socket\n");
           break;
        }
        else
        {
           perror("recv error");
           return EXIT_FAILURE;
        }
     } while (1);
     close (new_socket);
  }
  closedir (dir);
  close (create_socket);
  return EXIT_SUCCESS;
}

FILE* getFile(char* f)
{
	struct stat attribut;
	unsigned long sizeOfFile = 0;
	char choice[1];
	listChar(f);
	fprintf(stdout, "%s", f);
	if(stat(f, &attribut) == -1)
	{
		fprintf(stderr, "Fehler bei stat\n");
		return NULL;
	}
	else 
	{
		sizeOfFile = attribut.st_size;
		printf("Size of selected File %s is %ld bytes\n", f,sizeOfFile );
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

void listChar(char *f){
	int i;
	for(i = 0; f[i] != '\0'; i++) {
		fprintf(stdout, "%c - %i\n", f[i], f[i]);
	}
}