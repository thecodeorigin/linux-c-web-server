// INCLUDES 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h> 

// DEFINES 
#define MAX_CONNECTIONS 100
#define PATH_MAX 4096 

// GLOBAL SEC. 
char ROOT[PATH_MAX];
int BUFFER_SIZE = 51200; // 50Kb = default page table size
int PORT = 3000;
// int PORT = 31337;

char *responses[] = 
{
	"HTTP/1.1 200 OK\n",
	"HTTP/1.0 400 Bad Request\n",
	"HTTP/1.0 403 Forbidden\n",
	"HTTP/1.0 404 Not Found\n",
};	

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
///////////////////////////////////////////////////////////////////////////////////////////////


void getClientAddr(int sock_fd)
{
	char clientip[200];
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	

	int res = getpeername(sock_fd,(struct sockaddr *)&addr, &addr_size);
	strcpy(clientip, inet_ntoa(addr.sin_addr));

	printf("Client connected. [IP addr: %s]\n",clientip);
}
///////////////////////////////////////////////////////////////////////////////////////////////


void handle_request(char *client_request, int sock_fd)
{
	char *reqline[3], path[PATH_MAX], data_to_send[BUFFER_SIZE];
	int bytes_read, bytes_write, temp, fd, c;
	size_t content_length;
	FILE *fp;

	reqline[0] = malloc(sizeof(strlen(client_request)));
	reqline[1] = malloc(BUFFER_SIZE);
	reqline[2] = malloc(BUFFER_SIZE);
	

	// parser the request received from client
	printf("\033[1;92mPARSING REQUEST...\033[0m\n");
	// printf("%s",client_request);

	// reqline[0] = http method
	// reqline[1] = file required
	// reqline[2] = protocol version
	reqline[0]	= strtok(client_request," ");
	reqline[1] 	= strtok(NULL," ");
	reqline[2] 	= strtok(NULL,"\r\n");
	

	// ONLY accepts HTTP/1.0 and HTTP/1.1 request headers from client
	// Persistent connection is an opt-in feature, anyway 
	if (strncmp(reqline[2], "HTTP/1.0", 8)!=0 && strncmp(reqline[2], "HTTP/1.1", 8)!=0 )
	{

		send(sock_fd, responses[2], strlen(responses[2]), 0);
		return;
	}
	
	
	printf("reqline[0]	= %s \n",reqline[0]);
	printf("reqline[1]	= %s \n",reqline[1]);
	printf("reqline[2]	= %s \n",reqline[2]);
	

	/*	****************************** HANDLE HTTP METHODS ****************************** */
	//	GET METHOD
	if(strcmp(reqline[0],"GET") == 0)
	{		
		if ( strncmp(reqline[1], "/\0", 2)==0 )
			// If no file is specified, index.html will be opened by default   
			// strcpy(reqline[1],"/cat.jpg");
			strcpy(reqline[1],"/www/index.html");

	
		// printf("ROOT = %s \n",ROOT);	
		strcpy(path, ROOT);
		strcpy(&path[strlen(ROOT)], reqline[1]);
		printf("File Required = %s\n", path);
	
		
		bytes_read = 0;
		bytes_write = 0;
		temp = 0;
		if (fp = fopen(path, "r" ))	//FILE FOUND
		{
			fd = fileno(fp);

			c = fgetc(fp);
			while(c != EOF)
			{
				// data_to_send[bytes_read] = c;
				bytes_read++;
				c = fgetc(fp);
			}
			// data_to_send[bytes_read] = '\0';
			rewind(fp);

			char str[4];
			sprintf(str, "%d", bytes_read);
			send(sock_fd, responses[0], strlen(responses[0]), 0);
			send(sock_fd, "Connection: keep-alive\n", strlen("Connection: keep-alive\n"), 0);
			send(sock_fd, "Content-Length: ", strlen("Content-Length: "), 0);
			send(sock_fd, str, strlen(str), 0);
			send(sock_fd, "\n", 1, 0);
			// send(sock_fd, "Keep-Alive: max=3\n", strlen("Keep-Alive: max=3\n"), 0);
			send(sock_fd, "Keep-Alive: timeout=3\n\n", strlen("Keep-Alive: timeout=3\n\n"), 0);
			// send(sock_fd, "\n\n", 2, 0);

			
			bytes_read = 0;
			while ( (temp = read(fd, data_to_send, BUFFER_SIZE)) > 0 )
			{
				
				bytes_read += temp;
				bytes_write += write(sock_fd, data_to_send, temp);
			}
			// bytes_write += write(sock_fd, data_to_send, bytes_read);
			fclose(fp);	
			
		
			printf("bytes_read = %d\n", bytes_read);
			printf("bytes_write = %d\n\n", bytes_write);
		}
		else
		{
			write(sock_fd, responses[1], strlen(responses[1])); //FILE NOT FOUND
			printf("File not found.\n");
			return;
		}
	}
	

	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////


void * respond(void *arg)
{
	int i, j, bytes_read, crlf_count, singleByte, sock_fd, total_requests;
	char clientRequest[BUFFER_SIZE], singleChar[1];
	clock_t t;
	
	// get socket FD 
	sock_fd = *((int *)arg);
	printf("#[sock_fd]: %d\n", sock_fd);
	
	// initialize consumer variables
	memset((void*)clientRequest, (int)'\0', BUFFER_SIZE);
	singleChar[0] = '\0';
	bytes_read = 0;
	crlf_count = 0;
	total_requests = 0;
	i = 0, j = 0;

	// read data from client socket until find 2x CRLF's, or the timeout param value has achieved their limit
	// default timeout set to 3 sec's
	while (1)
	{	
		
		if(j == 0)
			printf("\n\033[1;35mRECEIVING HEADERS...\033[0m\n");
		
		// start timeout mark
		// t = clock();
	
		singleByte = recv(sock_fd, singleChar, 1, 0);
		if (singleByte < 0) // receive error
		{
			fprintf(stderr,"Error receiving data from client.\n");
			break;
		}
		else if (singleByte == 0) // socket closes
		{
			fprintf(stderr,"Client disconnected unexpectedly.\n");
			break;
		}
		bytes_read++;


		// clients header already received here
		// verifys '\r\n\r\n' sequence
		// if(bytes_read == )
		if(singleChar[0] == '\n' && clientRequest[i - 2] == '\n')
		{						
		
			clientRequest[i] = singleChar[0];
			printf("%c",clientRequest[i]);
			// printf("#[bytes_read] CLIENT REQUEST HEADER = %d\n", bytes_read);


			// handle request type properly
			handle_request(clientRequest, sock_fd);
			
			// prepare to a new request header
			memset((void*)clientRequest, (int)'\0', BUFFER_SIZE);
			bytes_read = 0;
			crlf_count = 0;
			i = 0;
			j = 0;
			continue;		
		}

		else if(singleChar[0] == '\n' && crlf_count == 0)
		{
			clientRequest[i] = singleChar[0];
			printf("%c",clientRequest[i]);
			crlf_count = 1;
			j = 1;
			i++;
			continue;	
		}

		else
		{
			clientRequest[i] = singleChar[0];
			printf("%c",clientRequest[i]);
			j = 1;
			i++;
			continue;	
		}
	}


	shutdown(sock_fd, SHUT_RDWR);
	close(sock_fd);
	pthread_exit(&sock_fd);
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    pthread_t threads[MAX_CONNECTIONS];
    struct sockaddr_in clientAddr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int server_fd, sock_fd;
    int i = 0;
	// int *ptr[MAX_CONNECTIONS];

	
	// If port number is not specified, set default http port
	PORT = ((argv[1] != (char *)' ') && (argc >= 2)) ? atoi(argv[1]) : PORT;

	// ROOT to serve files
	strcpy(ROOT,getenv("PWD"));
    
    // Creating socket file descriptor
    // if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(1);
    }
	
	clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = htons(PORT);

	// Attach socket to the clientAddr:port specified 
    if (bind(server_fd, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
    {
        perror("bind failed");
        exit(1);
    }
	
	// char content_length[BUFFER_SIZE];
	// strcpy(content_length,"BANANA = ");
	// int size = strlen(content_length);
	// printf("\nPLANO = %s\n",content_length);
	// // printf("\nPLANO = %s\n",strcat("Content-Length: ",(char *)itoa(strlen("adasdas"))));
	// exit(0);

	// run the server
    while (1)
    {
		if (listen(server_fd, MAX_CONNECTIONS) < 0)
        {
            perror("listen failed");
            exit(1);
        }
		
        printf("\033[1;37mLISTENING CONNECTION... # %d \033[0m\n",i);
        if ((sock_fd = accept(server_fd, (struct sockaddr *)&clientAddr, (socklen_t *)&addrlen)) < 0)
        {
            
			perror("Cannot accept connection");
			exit(1);
        }
		// else
		// {
		// 	int enable = 1;
		// 	if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    	// 		perror("setsockopt(SO_REUSEADDR) failed");
		// }
		
	
		// Get and prints client address 
        getClientAddr(sock_fd);

		// SEQUENCIAL 
		// respond(&sock_fd);

		// MULTITHREAD
		// Create thread and handle request
        if (pthread_create(&threads[i % MAX_CONNECTIONS], NULL, &respond, &sock_fd) != 0)
		{
			fprintf(stderr, "error: Cannot create thread # %d\n", i);
			break;	
		}

		//	BEGIN OF CRITICAL SECTION
		// pthread_mutex_lock(&lock); 
			
		// pthread_mutex_unlock(&lock);
		//	END OF CRITICAL SECTION
	
		// Wait thread finish their job
		if (pthread_join(threads[i % MAX_CONNECTIONS], NULL) != 0)
        {
			fprintf(stderr, "error: Cannot join thread # %d\n", i);
			break;
        }	
		else 
		{	
			
			printf("\033[1;31mTHREAD [# %d] HAS SUCCESSFULLY FINISHED THEIR JOB. \033[0m \n\n",i);
		}

		// increments # total conns
		i++;
    }



    return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////
