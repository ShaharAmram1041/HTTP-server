#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
/* output defines */
#define BAD_REQUEST "400 Bad Request"
#define NOT_SUPPORTED "501 Not supported"
#define NOT_FOUND "404 Not Found"
#define FOUND "302 Found"
#define OK "200 OK"
#define FORBIDDEN "403 Forbidden"
#define ERR "500 Internal Server Error"

int checkValidInput(int,char**);
int checkThePort(char*);
int checkNeg(char*);
int handler_function(void*);
int checkNumberOfTokens(char*);
int lastToken(char*);
int checkTheMethod(char*);
int checkThePath(char*);
int checkThePathDir(char*);
int PermissionCheck(char*);
void Response_Function(char* , char* , int );
char *get_mime_type(char*);
void create_200_response(char*,char*,int,int);
void create_response(char* ,char* ,char*,int,char*,int);

int main(int argc,char** argv){
    /*check if the input command legal*/
    if(checkValidInput(argc,argv) == -1){
        printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
        return 0;}

    int port = atoi(argv[1]);
    int pool_size = atoi(argv[2]);
    int max_num_of_req = atoi(argv[3]);

    /* create the thread-pool */
    threadpool *p = create_threadpool(pool_size);
    if(p == NULL){
        printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
        return 0;
    }

    /* create the socket */
    int fd;
    if((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        destroy_threadpool(p);
        exit(1);}

    struct sockaddr_in srv;
    srv.sin_family = AF_INET; /* use the Internet addr family */
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    srv.sin_port = htons(port); /* bind socket ‘fd’ to command port */

    /* bind: a client may connect to any of my addresses */
    if(bind(fd, (struct sockaddr*) &srv, sizeof(srv)) < 0) {
        perror("bind");
        destroy_threadpool(p);
        exit(1);}

    /* indicates that the server will accept a connection */
    if(listen(fd, 5) < 0) {
        perror("listen");
        destroy_threadpool(p);
        exit(1);}

    /* create array of int to keep each socket from server */
    int* fdArr = (int*) malloc(sizeof (int) * max_num_of_req);
    if(fdArr == NULL){
        perror("Malloc fail");
        destroy_threadpool(p);
        exit(1);
    }
    int i = 0;
    while (max_num_of_req > 0) {
        /* Accept the data packet from client and verification */
        *(fdArr + i) = accept(fd, (struct sockaddr *) NULL, NULL);
        if (*(fdArr + i) < 0) {
            destroy_threadpool(p);
            free(fdArr);
            close(fd);
            exit(0);
        }
        dispatch(p, handler_function, fdArr + i);
        max_num_of_req--;
        i++;
    }
    /* collecting all thread and free them */
    destroy_threadpool(p);
    /* close the welcome socket */
    close(fd);
    free(fdArr);
}

int checkValidInput(int argc,char** argv){
    /* illegal command length */
    if(argc != 4)
        return -1;
    /* illegal port */
    if(checkThePort(argv[1]) == -1)
        return -1;
    /* negative number */
    if(checkNeg(argv[2]) == -1 || checkNeg(argv[3]) == -1)
        return -1;
    return 0;
}

/* check if the port is bigger than 2^16 */
int checkThePort(char *port){
    int power = 1;
    int res = 0;
    int size = (int)(strlen(port));
    if( size > 6)
        return -1;
    for(int r = size- 1; r >= 0; r--){
        if (!(isdigit(port[r])))
            return -1;
        int sum = ((int)(port[r]) -48) * power;
        res += sum ;
        power *= 10;}
    if(res > 65536 || res < 0)
        return -1;
    return 0;
}

/* check if the number is negative */
int checkNeg(char *str){
    int power = 1;
    int res = 0;
    int size = (int)(strlen(str));
    for(int r = size- 1; r >= 0; r--){
        if (!(isdigit(str[r])))
            return -1;
        int sum = ((int)(str[r]) -48) * power;
        res += sum ;
        power *= 10;}
    return 0;
}

/* execute of each thread */
int handler_function(void *arg){
    signal(SIGPIPE, SIG_IGN);
    if(arg == NULL)
        return -1;
    int* fd = (int *) (arg);
    if(*fd < -1)
        return -1;
    /* reading from the socket */
    int read_from_cli = 0;
    char *cli_req = (char*)malloc(sizeof(char));
    if(cli_req == NULL){
        create_response(ERR,NULL,NULL,*fd,NULL,0);
        close(*fd);
        return 0;
    }
    cli_req[0] = '\0';
    char buffer[500];
    size_t size = 0;
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        read_from_cli = (int) read(*fd, buffer, sizeof(buffer) - 1);

        if (read_from_cli < 0) {
            perror("ERROR reading from socket");
            create_response(ERR, NULL, NULL, *fd, NULL, 0);
            free(cli_req);
            close(*fd);
            return 0;
        }

        /* read till the end of the request */
        if (read_from_cli == 0) {
            break;
        }

        size += read_from_cli;
        cli_req = (char *) realloc(cli_req, size + 1);
        if (cli_req == NULL) {
            perror("realloc failed");
            create_response(ERR, NULL, NULL, *fd, NULL, 0);
            close(*fd);
            return 0;
        }
        memcpy(cli_req + size - read_from_cli, buffer, read_from_cli);
        cli_req[size] = '\0';

        /* check if request is complete */
        if (strstr(cli_req,"\r\n") != NULL) {
            break;
        }
    }

    char *ptr = cli_req;
    /* client request without \r\n */
    cli_req = strtok(cli_req,"\r\n");

    /* check the client request: */
    /* if there is no input just enter */
    if(cli_req == NULL){
        create_response(BAD_REQUEST,NULL,NULL,*fd,NULL,-1);
        free(ptr);
        close(*fd);
        return 0;
    }

    /* 400 bad request =>  check the 3 tokens, last token HTTP protocol */
    if(checkNumberOfTokens(cli_req) == -1 || lastToken(cli_req) == -1){
        Response_Function(cli_req,BAD_REQUEST,*fd);
        close(*fd);
        free(cli_req);
        return 0;
    }
    /* 501 not supported =>  check if the first token is GET */
    if(checkTheMethod(cli_req) == -1){
        Response_Function(cli_req,NOT_SUPPORTED,*fd);
        close(*fd);
        free(cli_req);
        return 0;
    }

    char path[500];
    char *path_ptr;
    path_ptr = strtok(cli_req+4, " ");
    if(path_ptr == NULL) {
        create_response(ERR, NULL, NULL, *fd, NULL, 0);
        return 0;
    }

    strcpy(path, path_ptr);
    if(path[0] != '/') {
        memmove(path, path + 1, strlen(path));
    } else {
        strcpy(path, "./");
        strcat(path,path_ptr+1);
        path[strlen(path)] = '\0';
    }

    /* 404 Not Found =>  check the path */
    if(checkThePath(path) == -1){
        Response_Function(cli_req,NOT_FOUND,*fd);
        close(*fd);
        free(cli_req);
        return 0;
    }
    /* 302 Found =>  check the if the path is directory and without '/' in the end */
    if(checkThePathDir(path) == -1){
        Response_Function(cli_req,FOUND,*fd);
        close(*fd);
        free(cli_req);
        return 0;
    }
    struct stat path_stat;
    stat(path, &path_stat);
    /* the given path is a directory */
    if (S_ISDIR(path_stat.st_mode) != 0 ){
            Response_Function(cli_req,OK,*fd);
    }

    /* the given path is a file */
    if(S_ISREG(path_stat.st_mode) != 0){
        /* the caller has no permissions */
        if(PermissionCheck(path) == -1)
            Response_Function(cli_req,FORBIDDEN,*fd);
        /* there is permission, returning the file */
        else
            Response_Function(cli_req,OK,*fd);
        close(*fd);
        free(cli_req);
        return 0;
    }
    close(*fd);
    free(cli_req);
    return 0;
}

int checkNumberOfTokens(char* cli_req){
    if(strlen(cli_req) < 14)
        return -1;
    int count = 0;
    for(int i = 0; i < strlen(cli_req); i++){
        if(cli_req[i] == ' ')
        if(i-1 >= 0 && i+1 < strlen(cli_req) && cli_req[i] == ' ' && cli_req[i-1] != ' ' && cli_req[i+1] != ' ')
            count++;
    }
    if(count != 2)
        return -1;
    return 0;
}
int lastToken(char* cli_req){
    int i;
    for(i = (int)strlen(cli_req) -1;i >=0; i--){
        if(cli_req[i] == ' ')
            break;
    }
    char arr[strlen(cli_req) - i];
    strcpy(arr, cli_req + i + 1);
    arr[strlen(arr) ] = '\0';
    if(strcmp(arr,"HTTP/1.0") != 0 && strcmp(arr,"HTTP/1.1") != 0)
        return -1;
    return 0;
}

int checkTheMethod(char* cli_req){
    if(cli_req[0] != 'G' || cli_req[1] != 'E' || cli_req[2] != 'T')
        return -1;
    if(cli_req[3] != ' ')
        return -1;
    return 0;
}
int checkThePath(char* path){
    struct stat buffer;
    if(stat(path,&buffer) != 0 )
        return -1;
    return 0;
}
int checkThePathDir(char* path){
    struct stat path_stat;
    stat(path, &path_stat);
    /* the given path is a directory */
   if (S_ISDIR(path_stat.st_mode) != 0 )
       if(path[strlen(path) - 1] != '/')
           return -1;
   return 0;
}
int PermissionCheck(char* path){
    struct stat statRes;
    int flag = 0;
    int i ;
    char* file = (char*) malloc(sizeof(char) * ((int)strlen(path)+1));
    file[0] = '\0';
    /* check each path's directories if they have X permission*/
   for(i = 0; i < (int) strlen(path);i++) {

       /* finding the directory every time */
       if ((path[i] == '/' || path[i+1] == '\0') && i != 0) {
           if(path[i+1] == '\0'){
                strcpy(file,path);
                file[(int)strlen(path)] ='\0';}
           else{
               strncpy(file,path,i);
               file[i] ='\0';
           }
           stat(file, &statRes);
           /* check for read permission if it is a file*/
           if(S_ISREG(statRes.st_mode)){
               if(!(statRes.st_mode & S_IROTH)){
                   flag = -1;
               }
               break;
           }
           /* check execute permission for each directory  */
           if (!(statRes.st_mode & S_IXOTH)) {
               flag = -1;
               break;
           }
       }
   }
    free(file);
    return flag;
}

void Response_Function(char* cli_req, char* typeOfError, int fd){
    char path[500];
    char *path_ptr;

    path_ptr = strtok(cli_req+4, " ");

    strcpy(path, path_ptr);
    if(path[0] != '/') {
        memmove(path, path + 1, strlen(path));
    } else {
        strcpy(path, "./");
        strcat(path,path_ptr+1);
        path[strlen(path)] = '\0';
    }

    char* type = get_mime_type(path);
    if(strcmp(OK,typeOfError) != 0){
        create_response(typeOfError,type,path,fd,NULL,-1);
    }

    /* 200 ok response */
    if(strcmp(OK,typeOfError) == 0){
        struct stat path_stat;
        stat(path, &path_stat);

        /* the path is directory */
        if(S_ISDIR(path_stat.st_mode) != 0) {
            /* copy the path */
            char* path1 = (char*)malloc(sizeof(char) * ((int)strlen(path) + 1));
            if(path1 == NULL){
                create_response(ERR,NULL,NULL,fd,NULL,0);
                return;
            }
            int i;
            for(i = 0; path[i] != '\0';i++){
                path1[i] = path[i];
            }
            path1[i] = '\0';
            /* add to the directory index.html and check if it's in*/
            strcat(path,"index.html");
            /* index.html found */
            if(stat(path,&path_stat) == 0){
                free(path1);
                type = get_mime_type(path);
                /* check for permission */
                if(PermissionCheck(path) == -1){
                    create_response(FORBIDDEN,type,path,fd,"0",-1);
                    return;
                }
                create_200_response(type, path, fd,1);}
            /* index.html not found */
            else{
                type = "text/html";
                create_200_response(type, path1, fd,0);
                free(path1);}
        }
        /* the path is a file with permissions */
        else{
            create_200_response(type, path, fd,2);
        }
        return;
    }
}

char *get_mime_type(char *name)
{
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}

/* 400,501,404,302,403,500,200 responses*/
void create_response(char* kindOf,char* type,char* path,int fd,char* length,int flag) {
    char *response = (char *) malloc(sizeof(char));
    response[0] = '\0';
    char len[4];
    char sentence[35];
    /* create the time */
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    /* 400 bad request */
    if (strcmp(kindOf, BAD_REQUEST) == 0) {
        strcpy(sentence, "Bad Request");
        sentence[strlen("Bad Request")] = '\0';
        strcpy(len, "113");
    }
    /* 501 not supported */
    if (strcmp(kindOf, NOT_SUPPORTED) == 0) {
        strcpy(sentence, "Method is not supported");
        sentence[strlen("Method is not supported")] = '\0';
        strcpy(len, "129");
    }
    /* 302 not found */
    if (strcmp(kindOf, NOT_FOUND) == 0) {
        strcpy(sentence, "File not found");
        sentence[strlen("File not found")] = '\0';
        strcpy(len, "112");
    }
    /* 403 forbidden */
    if (strcmp(kindOf, FORBIDDEN) == 0) {
        strcpy(sentence, "Access denied");
        sentence[strlen("Access denied")] = '\0';
        strcpy(len, "111");
    }
    /* 500 internal */
    if (strcmp(kindOf, ERR) == 0) {
        strcpy(sentence, "Some server side error");
        sentence[strlen("Some server side error")] = '\0';
        strcpy(len, "144");
    }
    response = (char*) realloc(response, strlen(response) + strlen("HTTP/1.0 ") + 1);
    strcpy(response, "HTTP/1.0 ");
    response = (char*) realloc(response, strlen(response) + strlen(kindOf) + strlen("\r\n") + strlen("Server: webserver/1.0\r\nDate: ") +
            strlen(timebuf) + 1);
    strcat(response, kindOf);
    strcat(response, "\r\n");
    strcat(response, "Server: webserver/1.0\r\nDate: ");
    strcat(response, timebuf);
    /*  only in 302 request */
    if (strcmp(kindOf, FOUND) == 0) {
        response = (char*) realloc(response, strlen(response) + strlen("\r\nLocation: ") + strlen(path) + strlen("/") + 1);
        strcat(response, "\r\nLocation: ");
        strcat(response, path);
        strcat(response, "/");
        strcpy(sentence, "Directories must end with a slash");
        sentence[strlen("Directories must end with a slash")] = '\0';
        strcpy(len, "123");
    }
    /* 200 ok */
    if(strcmp(kindOf,OK) == 0 ){
        struct stat b;
        char t[128] = "";
        if (!stat(path, &b))
            strftime(t, 128, RFC1123FMT, localtime(&b.st_mtime));

        /* int flag = 0, index.html not found */
        if(flag == 0){
            response = (char*) realloc(response, strlen(response) + strlen("\r\nContent-Type: text/html") + 1);
            strcat(response, "\r\nContent-Type: text/html");
        }
        else{
            if(type != NULL){
                response = (char*) realloc(response, strlen(response) + strlen("\r\nContent-Type: ") + strlen(type) + 1);
                strcat(response, "\r\nContent-Type: ");
                strcat(response, type);
            }}
        response = (char*) realloc(response, strlen(response) + strlen("\r\nContent-Length: ") + strlen(length)
        + strlen("\r\nLast-Modified: ") + strlen(t) + 1);
        strcat(response, "\r\nContent-Length: ");
        strcat(response, length);
        strcat(response, "\r\nLast-Modified: ");
        strcat(response, t);
    }
    /* it's not 200 ok */
    else{
        response = (char*) realloc(response, strlen(response) + strlen("\r\nContent-Type: text/html\r\nContent-Length: ") + strlen(len) + 1);
        strcat(response, "\r\nContent-Type: text/html\r\nContent-Length: ");
        strcat(response, len);
    }

    response = (char*) realloc(response, strlen(response) + strlen("\r\nConnection: close\r\n\r\n") + 1);
    strcat(response, "\r\nConnection: close\r\n\r\n");
    if(strcmp(kindOf,OK) == 0){
        response[(int)strlen(response)] = '\0';
        write(fd,response, (int)strlen(response));
        free(response);
        return;
    }
    response = (char*) realloc(response, strlen(response) + strlen("<HTML><HEAD><TITLE>")
    + strlen(kindOf) + strlen("</TITLE></HEAD>\r\n<BODY><H4>") + strlen(kindOf) +
            strlen("</H4>\r\n") + strlen(sentence) + strlen(".\r\n</BODY></HTML>\r\n") + 1);
    strcat(response,"<HTML><HEAD><TITLE>");
    strcat(response, kindOf);
    strcat(response, "</TITLE></HEAD>\r\n<BODY><H4>");
    if(strcmp(kindOf,BAD_REQUEST) == 0)
        strcat(response, "Bad request");
    else
        strcat(response, kindOf);
    strcat(response, "</H4>\r\n");
    strcat(response, sentence);
    strcat(response, ".\r\n</BODY></HTML>\r\n");
    response[(int)strlen(response)] = '\0';
    write(fd,response, (int)strlen(response));
    free(response);
}

void create_200_response(char* type,char* path,int fd,int flag) {
    /* represents the content of the file */
    unsigned char *contentFile = (unsigned char *) malloc(sizeof(unsigned char));
    if(contentFile == NULL){
        create_response(ERR,NULL,NULL,fd,NULL,0);
        return;
    }
    contentFile[0] = '\0';
    int Content_Length = 0;
    char t[128] = "";
    /* 200 ok with path is directory and index.html */
    if(flag == 1){
        FILE *fp = fopen(path, "r");
        if (fp == NULL) {
            perror("File cant open");
            return;
        }
        char ch;
        do {
            ch = fgetc(fp);
            contentFile = (unsigned char *) realloc(contentFile, (Content_Length + 1) * sizeof(char));
            contentFile[Content_Length] = ch;
            Content_Length++;
        } while (ch != EOF);
        /* Closing the file */
        fclose(fp);
        contentFile[Content_Length - 1] = '\0';
        char length[20];
        sprintf(length, "%d", Content_Length);
        create_response(OK,type,path,fd,length,1);
        write(fd,contentFile, Content_Length);
    }

    /* the path is directory and index.html not in */
    if(flag == 0){
        /* get all the files in the directory */
        char** Names_Files_Dir = (char**) malloc(sizeof (char*));
        if(Names_Files_Dir == NULL){
            create_response(ERR,NULL,NULL,fd,NULL,0);
            return;
        }
        /* represent the content */
        char* content = (char*) malloc(sizeof(char) * 99999);
        if(content == NULL){
            create_response(ERR,NULL,NULL,fd,NULL,0);
            return;
        }
        content[0] = '\0';
        int i = 0 ,j = 0;
        int sizeOfRea = 1;
        DIR *d;
        struct dirent *dir;
        d = opendir(path);
        if(d){
            while((dir = readdir(d)) != NULL){
                Names_Files_Dir = (char**) realloc(Names_Files_Dir,sizeof (char*) * sizeOfRea);
                Names_Files_Dir[i] = (char*) malloc(sizeof(char) * (strlen(dir->d_name) + 1));
                strcpy(Names_Files_Dir[i],dir->d_name);
                Names_Files_Dir[i][strlen(dir->d_name)] = '\0';
                i++;
                sizeOfRea++;
            }
        }
        closedir(d);
        strcat(content,"<HTML>\r\n<HEAD><TITLE>Index of ");
        strcat(content,path);
        strcat(content,"</TITLE></HEAD>\r\n\r\n<BODY>\r\n<H4>Index of ");
        strcat(content,path);
        strcat(content,"</H4>\r\n\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>"
                       "Last Modified</th><th>Size</th></tr>\r\n\r\n");
        while(i > 0){
            struct stat b1;
            t[0] ='\0';
            char* m =(char*) malloc(sizeof(char) * ((int)strlen(path) + strlen(Names_Files_Dir[j]) +1));
            if(m == NULL){
                create_response(ERR,NULL,NULL,fd,NULL,0);
                return;
            }
            strcpy(m,path);
            strcat(m,Names_Files_Dir[j]);
            m[strlen(m)] ='\0';
            if (!stat(m, &b1))
                strftime(t, sizeof(t), RFC1123FMT, localtime(&(b1.st_ctime)));
            strcat(content,"<tr>\r\n<td><A HREF=\"");
            strcat(content,Names_Files_Dir[j]);
            strcat(content,"\">");
            strcat(content,Names_Files_Dir[j]);
            strcat(content,"</A></td><td>");
            strcat(content,t);
            strcat(content,"</td>\r\n<td>");
            if(stat(m,&b1) != -1){
                /* if its a regular file */
                if(S_ISREG(b1.st_mode) != 0){
                    int s = (int)b1.st_size;
                    char str[50];
                    sprintf(str,"%d",s);
                    strcat(content,str);
                }}
            strcat(content,"</td>\r\n</tr>\r\n");
            i--;
            j++;
            free(m);
        }
        strcat(content,"\r\n</table>\r\n\r\n<HR>\r\n\r\n"
                       "<ADDRESS>webserver/1.0</ADDRESS>\r\n\r\n</BODY></HTML>\r\n");
        content[strlen(content)] = '\0';
        Content_Length = (int)strlen(content);
        char length[20];
        sprintf(length, "%d", Content_Length);
        create_response(OK,type,path,fd,length,0);
        write(fd,content,(int)strlen(content));
        free(content);
        for(i = 0; i < sizeOfRea - 1; i++)
            free(Names_Files_Dir[i]);
        free(Names_Files_Dir);
    }
    /* 200 ok */
    if(flag == 2) {
        struct stat file_status;
        stat(path, &file_status);
        Content_Length = (int)file_status.st_size;
        if(contentFile == NULL){
            create_response(ERR,NULL,NULL,fd,NULL,0);
            return;
        }
        int i = 0;
        FILE *fp = fopen(path, "r");
        if (fp == NULL) {
            create_response(ERR,NULL,NULL,fd,NULL,0);
            return;
        }
        unsigned char read_from[3000];
        char length[20];
        sprintf(length, "%d", Content_Length);
        create_response(OK,type,path,fd,length,1);
        int nBytes = 0;
        /* read the file */
        fseek(fp,i,SEEK_SET);
        while((nBytes = (int)fread(&read_from, sizeof(unsigned char), sizeof(read_from) ,fp)) > 0){
            write(fd,read_from,nBytes);}
        fclose(fp);
    }
    free(contentFile);
}
