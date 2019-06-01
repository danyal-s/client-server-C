#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#define BUFFER_SIZE 4096

pthread_mutex_t mutexor = PTHREAD_MUTEX_INITIALIZER;
pthread_t t1, t2;
static char* tbuffer[BUFFER_SIZE];
static int sock;
static int messages = 0;
void* writer(void* arg) //This thread will write to the buffer, tbuffer
{
    while(1)
    {
        char buffer[BUFFER_SIZE];
        char checknull;
        ssize_t counted = 0;
        char* bufptr = buffer;
        do
        {
            ssize_t blue = read(sock, bufptr, BUFFER_SIZE - counted);
            counted = counted + blue;
            checknull = buffer[counted - 1];
            bufptr = bufptr + blue;
        }while(checknull != '\0' && checknull != '\n');
        buffer[counted] = '\0';
        int i;

        for(i = 0; i < BUFFER_SIZE; i++)
        {
            if(tbuffer[i] == NULL)
            {
                pthread_mutex_lock(&mutexor);
                tbuffer[i] = (char*)malloc(strlen(buffer) + 1);
                strcpy(tbuffer[i], buffer);
                messages++;
                pthread_mutex_unlock(&mutexor);
                break;
            }
        }
        if(strcasecmp("Goodbye.\n", buffer) == 0)
            pthread_exit(NULL);
    }

}
void* reader(void* arg)
{
    char buffer[BUFFER_SIZE];
    do
    {
        write(STDOUT_FILENO, "\nInput: ", 8);
        ssize_t count = read(STDIN_FILENO, buffer, BUFFER_SIZE);
        buffer[count - 1] = '\0';
        if(strcasecmp(buffer, "disconnect") == 0)
            pthread_exit(0);
        if (write(sock, buffer, count) < 0)
            perror("writing on stream socket");
        usleep(5000);
        int i;
        if(messages > 0)
        {
            messages = 0;
            for(i = 0; i < BUFFER_SIZE; i++)
            {
            pthread_mutex_lock(&mutexor);
                if(tbuffer[i] != NULL)
                {
                    int errval;
                    char *tbuffptr = tbuffer[i];
                    int sizing = strlen(tbuffer[i]);
                    while(sizing > 0)
                    {
                        ssize_t red;
                        do
                        {
                            red = write(STDOUT_FILENO, tbuffptr, sizing);
                            errval = errno;
                        }while(red < 0 && (errval == EWOULDBLOCK || errval == EINTR || errval == EAGAIN));
                        sizing = sizing - red;
                        tbuffptr = tbuffptr + red;
                    }
                    bzero(tbuffer[i], strlen(tbuffer[i]));
                    free(tbuffer[i]);
                    tbuffer[i] = NULL;
                }
            pthread_mutex_unlock(&mutexor);
            }
        }
    }while(strcasecmp(buffer, "exit") != 0);
    pthread_exit(NULL);
}
int main(int argc, char *argv[])
{
char mainbuff[BUFFER_SIZE];
int *k = (int*)malloc(sizeof(int));
do
{
	struct sockaddr_in server;
	struct hostent *hp;
	pthread_attr_t tattrib;
	if(argc == 3)
	{
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0) {
			perror("opening stream socket");
			exit(1);
		}

		server.sin_family = AF_INET;
		hp = gethostbyname(argv[1]);
		if (hp == 0) {
			fprintf(stderr, "%s: unknown host\n", argv[1]);
			exit(2);
		}
		bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
		server.sin_port = htons(atoi(argv[2]));

		if (connect(sock,(struct sockaddr *) &server,sizeof(server)) < 0) {
			perror("connecting stream socket");
			exit(1);
		}
	}
	else if(argc == 1)
	{
        write(STDOUT_FILENO, "#", 1);
		ssize_t counter = read(STDIN_FILENO, mainbuff, BUFFER_SIZE);
		mainbuff[counter - 1] = '\0';
		char* pointer = strtok(mainbuff, " ");
		int flag = 0;
		if(pointer != NULL)
		{
            if(strcasecmp(pointer, "connect") == 0)
            {
                pointer = strtok(NULL, " ");
                if(pointer != NULL)
                {	sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (sock < 0)
                    {
                        perror("opening stream socket");
                        exit(1);
                    }
                    server.sin_family = AF_INET;
                    hp = gethostbyname(pointer);
                    if (hp == 0)
                    {
                        write(STDERR_FILENO, "Unknown host.\n", 14);
                        continue;
                    }
                    bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
                    pointer = strtok(NULL, " ");
                    if(pointer != NULL)
                    {
                        int val = atoi(pointer);
                        if (val == 0)
                        {
                            write(STDOUT_FILENO, "Please input a number for the port.\n", 37);
                            continue;
                        }
                        server.sin_port = htons(val);
                        if (connect(sock,(struct sockaddr *) &server,sizeof(server)) < 0)
                        {
                            write(STDERR_FILENO, "Invalid port.\n", 14);
                        }
                        else
                        {
                            write(STDOUT_FILENO, "Connected.\n", 11);
                        }
                    }
                    else
                    {
                        write(STDOUT_FILENO, "Incomplete command.\n", 20);
                        continue;
                    }
                }
                else
                {
                    write(STDOUT_FILENO, "Incomplete command.\n", 20);
                    continue;
                }
            }
            else if(strcasecmp(pointer, "disconnect") == 0)
            {
                write(STDOUT_FILENO, "Not connected.\n", 15);
                continue;
            }
            else if(strcasecmp(pointer, "exit") == 0)
            {
                write(STDOUT_FILENO, "Goodbye.\n", 10);
                exit(EXIT_SUCCESS);
            }
            else
            {
                write(STDOUT_FILENO, "Unsupported command.\n", 21);
                continue;
            }
        }
        else
        {
            write(STDOUT_FILENO, "No input found.\n", 16);
            continue;
        }
	}
	else
	{
        write(STDOUT_FILENO, "Usage: client <IP Address> <Port Number>\n", 42);
	}
	pthread_attr_init(&tattrib);
	//pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_attr_setscope(&tattrib, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&t1, &tattrib, (void*)reader, NULL);
	pthread_create(&t2, &tattrib, (void*)writer, NULL);
    pthread_join(t1, (void*)k);
	//pthread_kill(t2, SIGTERM);
	close(sock);
}while(*k != 1);
return 0;
}
