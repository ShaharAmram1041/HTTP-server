#HTTP server
Authored by Shahar Amram


==Description==
The program is a HTTP server:
Constructs an HTTP response based on the client's request.
Sends the response to the client.

program contain 2 files:

threadpool.c:
The pool is implemented by a queue. When the server gets a connection (getting back from accept()), it should put the connection in the queue. When there will be available thread (can be immediate), it will handle this connection (read request and write response).
You should implement the functions in threadpool.h.
The server should first init the thread pool by calling the function create_threadpool(int).
This function gets the size of the pool.


server.c:
The main file.
Read request from the socket.
Check input: The request’s first line should contain a method, path, and protocol.
If that there are not 3 tokens or the last one is no of the HTTP versions return error message “400 Bad Request" response.
If the first token is not 'GET' return error message "501 not supported" response.
If the requested path does not exist, return the error message "404 Not Found" response.
If the path is a directory but it does not end with a '/', return error message “302 Found” response.
If the path is a directory and it ends with a '/',  we will search for index.html: if we found it and the caller has premissions we return the page, otherwise return the contents of the directory.
If the path is a file and the caller has no 'read' premissions return the error message “403 Forbidden” response, otherwise return the "200 OK" with the wanted file.
in any case of failed after the connection is set, return the "500 internal" error message.



Program DATABASE:
1.argv - type of char**, represent the input from the user from the command line.
2.argc - type of int, the size of argv.
3.fdArr - type of int*, represent each file descriptor from the accept function.
4.cli_req - type of char*, represent the client request.
5.response- type of char*, represnts the message answer from the server.
6.contentFile - type of unsigned char*, represent the content of the file that the client request.
7.Names_Files_Dir - type of char**, represent the files in the directory.



functions:

threadpool.c:
1.threadpool* create_threadpool(int num_threads_in_pool) - function that creates and initalize the threads by the number of thread in pool.
2.dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg) -dispatch enter a "job" of type work_t into the queue. when an available thread takes a job from the queue, it will call the function "dispatch_to_here" with argument "arg".
3.do_work(void* p) - The work function of the thread.
4.destroy_threadpool(threadpool* destroyme) -  destroy_threadpool kills the threadpool, causing all threads in it to commit suicide, and then frees all the memory associated with the threadpool.

server.c:
1.main(int ,char**) - the main of the program. contain the implements of the server.
2.checkValidInput(int,char**) - private function check if the input from the user legal.
3.checkThePort(char*) - private function, if the port legal.
4.checkNeg(char*) - private function, check if the input is without negative numbers.
5.handler_function(void*) - private function, the function each thread will do after the connection is set.
6.checkNumberOfTokens(char*) - private function, check the token's numbers of the given input.
7.lastToken(char*) - private function, check if the last toke is HTTP version.
8.checkTheMethod(char*) - private function, check if the first token is 'GET' method.
9.checkThePath(char*) - private function, check if the path exists.
10.checkThePathDir(char*) - private function, check if the path is a directory and not end with '/'.
11.PermissionCheck(char*) - private function, check the permission of given path.
12.Response_Function(char* , char* , int ) - private function, send each of the type errors to create the response error.
13.get_mime_type(char*) - function, get the type of path file.
14.create_200_response(char*,char*,int,int) - private function, create the content file of the "200 ok" response.
15.create_response(char* ,char* ,char*,int,char*,int) - private function, generic. create for each type the exact response.


==Program Files==
threadpool.c - the file contain the excute of the threads, the work they do and the destroy of them.
server.c - the file conatin the execute of HTTP server.


==How to compile?==
compile : gcc -g -Wall threadpool.c server.c -lpthread -o server
run: ./server

==Input:==
The input will be from the command line, 3 numbers represent the port, number of threads, maximum number of requests.

==Output:==

1. in failure in the command line, print "Usage: server <port> <pool-size> <max-number-of-request>\n".
2. in failure after conncetion is set, return "500 Internal Server Error".
3. in success, return the responses by the given path. (in case of 200 ok, return the wanting file).




