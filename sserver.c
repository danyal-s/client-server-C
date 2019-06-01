#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#define BUFFER_SIZE 4096
#define NUMBER_OF_PROCESSES 1024
#define _GNU_SOURCE

typedef enum
{
	on, off, neither
} STATE;

typedef struct process
{
	pid_t pid;
	char* name;
	time_t tstarted;
	time_t tended;
	STATE state;
} process;

static void fullreader(int fd, char buff[], size_t count)
{
    char* bufptr = buff;
    char checknull;
    size_t counted = 0;
    do
    {
        ssize_t blue = read(fd, bufptr, count - counted);
        counted = counted + blue;
        checknull = buff[counted - 1];
        bufptr = bufptr + blue;
    }while(checknull != '\0' && checknull != '\n');
    buff[counted] = '\0';
}
static void fullwriter(int fd, char buff[], size_t count)
{
    int errval;
    char* bufptr = buff;
    int sizing = count;
    while(sizing > 0)
    {
        ssize_t red;
        do
        {
            red = write(STDOUT_FILENO, bufptr, sizing);
            errval = errno;
        }while(red < 0 && (errval == EWOULDBLOCK || errval == EINTR || errval == EAGAIN));
        sizing = sizing - red;
        bufptr = bufptr + red;
    }
}
char* timegetter(time_t elapsed)
{
    struct tm* timestruct;
    timestruct = gmtime(&elapsed);
    char* buff = (char*)malloc(128);
    strftime(buff, 128, "%T", timestruct);
    return buff;
}
static void addprocess(pid_t pid, int* start, char* name, process runarray[], int* numproc)
{
	int index = *start;
	*start = *start + 1;
	runarray[index].pid = pid;
	runarray[index].name = (char*)malloc(strlen(name));
	strcpy(runarray[index].name, name);
	runarray[index].tstarted = time(0);
	runarray[index].state = on;
	*numproc = *numproc + 1;
}

static char* endprocess(pid_t pid, process runarray[], int* numproc, int index)
{
	if(kill(pid, SIGTERM) == -1)
	{
		if(errno == ESRCH)
		{
			return "Cannot find that process.\n";
		}
		else if(errno == EPERM)
		{
			return "You do not have permission to close that process.\n";
		}
	}
	else
	{
		runarray[index].state = off;
		runarray[index].tended = time(0);
		*numproc = *numproc - 1;
		return "Process sucessfully ended.\n";
	}
}

int main()
{

int sock, length;
struct sockaddr_in server;
int msgsock;
char buf[1024];
int rval;
int i;

sock = socket(AF_INET, SOCK_STREAM, 0);
if (sock < 0)
{
	perror("opening stream socket");
	exit(1);
}
server.sin_family = AF_INET;
server.sin_addr.s_addr = INADDR_ANY;
server.sin_port = 0;
if (bind(sock, (struct sockaddr *) &server, sizeof(server)))
{
	perror("binding stream socket");
	exit(1);
}
length = sizeof(server);
if (getsockname(sock, (struct sockaddr *) &server, &length))
{
	perror("getting socket name");
	exit(1);
}
printf("Socket has port #%d\n", ntohs(server.sin_port));
fflush(stdout);

listen(sock, 5);
while(1)
{
	msgsock = accept(sock, 0, 0);
	pid_t mainpid = fork();
	if(mainpid == 0)
	{
		char buffer[BUFFER_SIZE];
		struct process runningProcessArray[NUMBER_OF_PROCESSES];
		struct process exitedProcessArray[NUMBER_OF_PROCESSES];
		int start = 0;
		int end = NUMBER_OF_PROCESSES - 1;
		char *tokenized;
		int numProcesses = 0;
		if (msgsock == -1)
			perror("accept");

		int a;
		for(a = 0; a < NUMBER_OF_PROCESSES; a++)
		{
			runningProcessArray[a].name = "none";
			runningProcessArray[a].state = neither;
		}
		do
		{
			usleep(150000);
			read(msgsock, buffer, BUFFER_SIZE);
			char* inputcopy = (char*)malloc(sizeof(buffer));
			strncpy(inputcopy, buffer, sizeof(buffer));
			tokenized = strtok(buffer, " ");
			if(tokenized != NULL)
			{
				if(strcasecmp(tokenized, "run") == 0)
				{
					tokenized = strtok(NULL, " ");
					if(tokenized != NULL)
					{
						char* path = (char*)malloc(strlen(tokenized));
						strcpy(path, tokenized);
						tokenized = strtok(NULL, " ");
						if(tokenized == NULL)
						{
							int status;
							int runpipe[2];
							int errCheck = pipe2(runpipe, O_CLOEXEC);
							if(errCheck >= 0)
							{
								char* truepath = (char*)malloc(strlen(path));
								strcpy(truepath, path);
								char* pointing = strtok(path, "/"); //This string is processed to find the name of the process to send as the first argument when exec-ing.
								char* name = (char*)malloc(strlen(pointing));
								while(pointing != NULL)
								{
									free(name);
									name = (char*)malloc(strlen(pointing));
									strcpy(name, pointing);
									pointing = strtok(NULL, "/");
								}
								free(pointing);
								pid_t runpid = fork();
								if(runpid == 0)
								{
									char runcbuffer[64];
									close(runpipe[0]);
									execl(truepath, name, NULL);
									int errsv = errno;
									int ct = sprintf(runcbuffer, "%d", errsv);
									free(name);
									write(runpipe[1], runcbuffer, ct);
									close(runpipe[1]);
									exit(EXIT_FAILURE);
								}
								else if(runpid > 0)
								{
									char runbuffer[BUFFER_SIZE];
									close(runpipe[1]);
									int ct = read(runpipe[0], runbuffer, BUFFER_SIZE);
									if(ct == 0)
									{
										write(msgsock, "Successful.\n", 12);
										addprocess(runpid, &start, name, runningProcessArray, &numProcesses);
									}
									else
									{
										if(atoi(runbuffer) == ENOENT)
											write(msgsock, "Invalid Directory.\n", 19);
										else
										{
											write(msgsock, "Error while execing, error code: ", 33);
											write(msgsock, runbuffer, ct);
											write(msgsock, "\n", 1);
										}
									}
									close(runpipe[0]);
								}
								else //if it's -1
								{
									perror("Fork for run failed.");
								}
							}
							else
							{
								perror("Error in run pipe");
							}
						}
						else
						{
							write(msgsock, "Too many arguments.\n", 20);
						}
					}
					else
					{
						write(msgsock, "Incomplete command.\n", 20);
					}
				}
				else if(strcasecmp(tokenized, "add") == 0)
				{
					char* inputstring = (char*)malloc(strlen(inputcopy));
					strcpy(inputstring, inputcopy);
					strtok(inputstring, "+ ,");
					char* pointing = strtok(NULL, "+ ,");
					if(pointing == NULL)
					{
						write(msgsock, "Incomplete command.\n", 20);
					}
					else
					{
						long int sum = 0;
						int flag = 0;
						do
						{
							errno = 0;
							long int trans = strtol(pointing, NULL, 10);
							int errval = errno;
							if(errval == ERANGE)
							{
								write(msgsock, "Error: A number in the input is beyond 64-bits.\n", 48);
								flag = 1;
								break;
							}
							else if(trans == 0 && (strcasecmp(pointing, "0") != 0))
							{
								write(msgsock, "Input must contain only numbers.\n", 33);
								flag = 1;
								break;
							}
							sum = sum + trans;
						}
						while((pointing = strtok(NULL, "+ ,")) != NULL);
						if(flag == 0)
						{
							char* print = (char*)malloc(21);
							snprintf(print, 21, "%ld\n", sum);
							write(msgsock, print, strlen(print));
						}
					}
				}
				else if(strcasecmp(tokenized, "mult") == 0)
				{
					char* inputstring = (char*)malloc(strlen(inputcopy));
					strcpy(inputstring, inputcopy);
					strtok(inputstring, "* ,");
					char* pointing = strtok(NULL, "* ,");
					int ct = 0;
					if(pointing == NULL)
					{
						write(msgsock, "Incomplete command.\n", 20);
					}
					else
					{
						long int mult;
						int flag = 0;
						do
						{
							 errno = 0;
							 long int trans = strtol(pointing, NULL, 10);
							 int errval = errno;
							 if(errval == ERANGE)
							 {
								 write(msgsock, "Error: A number in the input is beyond 64-bits.\n", 48);
								 flag = 1;
								 break;
							 }
							 else if(trans == 0 && (strcasecmp(pointing, "0") != 0))
							 {
								 write(msgsock, "Input must contain only numbers.\n", 33);
								 flag = 1;
								 break;
							 }
							 if(ct == 0)
							 {
								ct = 1;
								mult = trans;
							 }
							 else
								mult = mult * trans;
						}
						while((pointing = strtok(NULL, "* ,")) != NULL);
						if(flag == 0)
						{
							char* print = (char*)malloc(21);
							snprintf(print, 21, "%ld\n", mult);
							write(msgsock, print, strlen(print));
						}
					}
				}
				else if(strcasecmp(tokenized, "div") == 0)
				{
					char* inputstring = (char*)malloc(strlen(inputcopy));
					strcpy(inputstring, inputcopy);
					strtok(inputstring, "/ ,");
					char* pointing = strtok(NULL, "/ ,");
					if(pointing == NULL)
					{
						write(msgsock, "Incomplete command.\n", 20);
					}
					else
					{
						long int div;
						int ct = 0;
						int flag = 0;
						do
						{
							 errno = 0;
							 long int temp = strtol(pointing, NULL, 10);
							 int errval = errno;
							 if(errval == ERANGE)
							 {
								 write(msgsock, "Error: A number in the input is beyond 64-bits.\n", 48);
								 flag = 1;
								 break;
							 }
							 else if(temp == 0 && (strcasecmp(pointing, "0") != 0))
							 {
								 write(msgsock, "Input must contain only numbers.\n", 33);
								 flag = 1;
								 break;
							 }
							 if(ct == 0)
							 {
								 ct = 1;
								 div = temp;
							 }
							 else if(temp != 0)
								div = div / temp;
							 else
							 {
								 write(msgsock, "Divide by 0 error.\n", 19);
								 flag = 1;
								 break;
							 }
						}
						while((pointing = strtok(NULL, "/ ,")) != NULL);
						if(flag == 0)
						{
							char* print = (char*)malloc(21);
							snprintf(print, 21, "%ld\n", div);
							write(msgsock, print, strlen(print));
							free(print);
						}
					}
				}
				else if(strcasecmp(tokenized, "exit") == 0)
				{
					write(msgsock, "Goodbye.\n", 9);
					close(msgsock);
					break;
				}
				else if(strcasecmp(tokenized, "list") == 0)
				{
                    char listbuffer[8];
					tokenized = strtok(NULL, " ");
					if(tokenized == NULL)
					{
						int i;
						if(numProcesses == 0)
						{
								write(msgsock, "No processes are running.\n", 26);
						}
						else
						{
							for(i = 0; i < NUMBER_OF_PROCESSES; i++)
							{
								process* temp = &runningProcessArray[i];
								char* tempname = temp->name;
								pid_t temppid = temp->pid;
								int pidct = snprintf(listbuffer, 8, "%d", temppid);
								if(temp->state == on)
								{
									write(msgsock, "pid: ", 5);
									write(msgsock, listbuffer, pidct);
									write(msgsock, " name: ", 7);
									write(msgsock, tempname, strlen(tempname));
									write(msgsock, "\n", 1);
								}
							}
						}
					}
					else if(strcasecmp(tokenized, "all") == 0)
					{
                        for(i = 0; i < NUMBER_OF_PROCESSES; i++)
                        {
                            process* temp = &runningProcessArray[i];
                            char* tempname = temp->name;
                            pid_t temppid = temp->pid;
                            int pidct = snprintf(listbuffer, 8, "%d", temppid);
                            if(strcasecmp(tempname, "none") != 0)
                            {
                                write(msgsock, "pid: ", 5);
                                write(msgsock, listbuffer, pidct);
                                write(msgsock, " name: ", 7);
                                write(msgsock, tempname, strlen(tempname));
                                if(temp->state == on)
                                {
                                    write(msgsock, " State: on", 10);
                                    write(msgsock, " Time Elapsed: ", 14);
                                    time_t telapsed = time(0) - temp->tstarted;
                                    char *timealot = timegetter(telapsed);
                                    write(msgsock, timealot, strlen(timealot));
                                    free(timealot);
                                }
                                else
                                {

                                    write(msgsock, "State: off", 11);
                                    write(msgsock, " Time Elapsed: ", 14);
                                    time_t telapsed = temp->tended - temp->tstarted;
                                    char *timealot = timegetter(telapsed);
                                    //strcpy(timealot, timegetter(telapsed));
                                    write(msgsock, timealot, strlen(timealot));
                                    free(timealot);
                                }
                                write(msgsock, "\n", 1);
                            }

                        }
					}
					else if(strcasecmp(tokenized, "details") == 0)
					{

					}
					else
					{
						write(msgsock, "Invalid command.\n", 17);
					}
				}
				else if(strcasecmp(tokenized, "close") == 0)
				{
                    tokenized = strtok(NULL, " ");
                    if(tokenized == NULL)
                    {
                        write(msgsock, "Incomplete command.\n", 20);
                    }
                    else
                    {
                        errno = 0;
                        long int tempnum = strtol(tokenized, NULL, 10);
                        int errval = errno;
                        if(errval == ERANGE)
                            write(msgsock, "Error: A number in the input is beyond 64-bits.\n", 48);
                        else if(tempnum == 0 && (strcasecmp(tokenized, "0") != 0))
                        {
							char* proname = (char*)malloc(strlen(tokenized));
							strcpy(proname, tokenized);
							tokenized = strtok(NULL, " ");
							if(tokenized == NULL)
							{	for(i = 0; i < NUMBER_OF_PROCESSES; i++)
								{
									process *temp = &runningProcessArray[i];
									if((temp->state) == on && strcasecmp(temp->name, proname) == 0)
									{
										char closebuff[128];
										strcpy(closebuff, endprocess(temp->pid, runningProcessArray, &numProcesses, i));
										write(msgsock, closebuff, sizeof(closebuff));
										break;
									}
								}
							}
							else if(strcasecmp(tokenized, "all") == 0)
							{
								for(i = 0; i < NUMBER_OF_PROCESSES; i++)
								{
									process *temp = &runningProcessArray[i];
									if((temp->state) == on && strcasecmp(temp->name, proname) == 0)
									{
										char closebuff[128];
										strcpy(closebuff, endprocess(temp->pid, runningProcessArray, &numProcesses, i));
										write(msgsock, closebuff, sizeof(closebuff));
									}
								}
							}
							else
							{
								write(msgsock, "Invalid command.\n", 17);
							}
                        }
                        else
                        {
                            char closebuff[128];
                            int closepid = -1;
                            for(i = 0; i < NUMBER_OF_PROCESSES; i++)
                            {
                                if(runningProcessArray[i].pid == tempnum) //finding if PID even exists in process list
                                {
                                     closepid = tempnum;
                                     break;
                                }
                            }
                            if(closepid != -1)
                            {
                                strcpy(closebuff, endprocess(tempnum, runningProcessArray, &numProcesses, closepid));
                                write(msgsock, closebuff, sizeof(closebuff));
                            }
                            else
                                write(msgsock, "Process not found.\n", 19);
                        }
                    }
				}
				else if(strcasecmp("disconnect", tokenized) == 0)
				{
					close(msgsock);
					exit(EXIT_SUCCESS);
				}
				else
				{
					write(msgsock, "Invalid command.\n", 17);
				}
			}
			else
			{
				write(msgsock, "Nothing entered.\n", 17);
			}
		}while(1);
		close(msgsock);
		break;
	}
	else if(mainpid > 0)
	{
		close(msgsock);
	}
	else
	{
		perror("Forking while accepting");
		exit(EXIT_FAILURE);
	}
}
return 0;
}
