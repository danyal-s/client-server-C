# Linux Client/Server Architecture implementation
This is my implementation of a client server architecture in C. The client is multithreaded to provide realtime interactivity.

## Architecture of the Program
### Data structure
Array of processes
A 'process' is a struct with the following information:
``` C
pid_t pid
char* name
time_t tstarted
time_t tended
STATE state
```
STATE is an enum with 3 possible values:
on → process is running.
off → process has been closed.
neither → there is no process occupying this spot.

### Threads
The server is single threaded.
The client is multithreaded.

In case of the client, it has 1 thread to read input from the server. After it reads input from the server, it checks if there are any pending messages to be printed that came from the server via a buffer called ‘tbuffer’, which is an array of character pointers. If there are any messages, it prints them, then asks the user for input again.

The other thread simply takes responses from the server and puts them into the buffer as soon as they come. If the first reply comes, it will put it in tbuffer[0] if it’s NULL. if tbuffer[0] isn’t NULL, it’ll check tbuffer[1] if it’s not NULL. If even that’s NULL, then tbuffer[2] and so on. After it finds the right spot (that is, it’s not NULL), it’ll store that string of response from the server in that index.

A client may connect via the following syntax (check for the # prompt):
connect <IP_ADDRESS> <PORT_NUMBER>
After that, it may disconnect (to later connect to another server if needed) via simply writing “disconnect”.
The client may also exit via the ‘exit’ command.

### Server Commands
1 - run <path>
2 - close <pid/name>     
3 - close <name> all     
4 - list    
5 - list all     
6 - list details     
7 - add <numbers>     
8 - div <numbers>     
9 - mult <numbers>     
10 - disconnect     
11 - exit

### How the server works
The server forks after ‘accept()’. The parent blocks on accept for new connections, and the child continues on to serve the client.

I will be mentioning some commands that I feel should be explained:

run → This command will fork() and try to exec the child. If the child fails to exec, it’ll write an error message back to the parent via ‘pipe2’. If exec succeeds, an entry is made in the process table and pipe2 has the O_CLOEXEC flag which will cause it to close and write nothing to the pipe and so the parent will read 0 characters due to which it will know that exec succeeded.

disconnect/exit → The server will destroy all processes of the client that disconnects from the server. Same implementation of both commands from the server’s side.

## Limitations
1. The client crashes if the server is prematurely closed via ‘CTRL-C’ due to SIGPIPE and then that followed by SIGTERM.
2. Some messages from the tbuffer (pending messages from server) get lost if the client sends messages too fast.
3. 1024 users can connect at a time.
4. run crashes when you just send ‘/’ as an input.
