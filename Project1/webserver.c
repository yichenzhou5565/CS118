#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/* Global varaibles */
const int MYPORT = 5001;        /* Port number */
const int BACKLOG = 10;         /* Pending connections queue size */

/* Helper functions */
void connection(int fd);
void error404(int fd);

int main()
{
    int sockfd, new_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_size;
    int pid;
    
    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        fprintf(stderr, "Error in socket() system call. %s\n", strerror(errno));
        exit(1);
    }
    
    /* Initialize */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MYPORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));
    
    /* Bind */
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == -1)
    {
        fprintf(stderr, "Error in bind().\n");
        exit(1);
    }
    
    /* listen */
    if (listen(sockfd, BACKLOG) == -1)
    {
        fprintf(stderr, "Error in listen().\n");
        exit(1);
    }
    
    /* accept loop */
    for(;;)
    {
        client_size = sizeof(struct sockaddr_in);
        new_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_size);
        if (new_fd == -1)
        {
            fprintf(stderr, "Error in accept().\n");
            continue;
        }
        
        pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "Error on fork().\n");
            exit(1);
        }
        else if (pid == 0)
        {
            close(sockfd);
            connection(new_fd);
            exit(0);
        }
        else
        {
            close(new_fd);
        }
        
    }
    
    return 0;
}
/**********************************************************************/
/**************************** END OF MAIN *****************************/
/**********************************************************************/

void connection(int fd)
{
    /* Initializa buffer and read() */
    char buffer[8192];
    int i=0;
    for (; i<8192; i++)
    {
        buffer[i] = '\0';
    }
    int size_read = read(fd, buffer, (8191)*sizeof(char));
    if ( size_read < 0)
    {
        fprintf(stderr, "Error in read() system call. %s\n", strerror(errno));
        exit(1);
    }
    buffer[size_read-1] = '\0';
    /* Go through the readed request */
//    char* ptr;                                  /* Iterator */
//    buffer[size_read-1] = '\0';
//
////    printf("*****\n\n");
////    for(i=0; i<size_read; i++)
////        printf("%c", buffer[i]);
//
//    ptr = strstr(buffer, " HTTP/");
//    if (ptr == NULL)
//    {
//        fprintf(stderr, "This is NOT an HTTP request.\n");
//        exit(1);
//    }
//
////    else
////    {
////        *ptr = '\0';
////        ptr = NULL;
////    }
//
//    char* ptr2;
//    ptr2 = strstr(buffer, "GET /");
//    if (ptr2 == NULL)
//    {
//        fprintf(stderr, "Cannot recognize request.\n");
//        exit(1);
//    }
//    ptr2 += 5;
    printf("* Request:\n");
    for(i=0; i<size_read; i++)
        printf("%c", buffer[i]);
    char* end;
    char* begin;
    begin = strstr(buffer, "GET /");
    begin = begin +5;
    end = strstr(begin, " HTTP/");
    if (begin==NULL || end==NULL)
    {
        printf("****\n\n");
        fprintf(stderr, "Error in getting filename.\n");
        exit(1);
    }
    
    /* Get file name */
    char f_name[1024];
    int len = end - begin;
    f_name[len] = '\0';
    memcpy(f_name, begin, len);
    
    /* Deal with whitespace in file name */
    char real_name[len+1];
    real_name[len] = '\0';
    int ind = 0;
    for(i=0; i<len; i++)
    {
        if (f_name[i] == '%')
        {
            memcpy(&real_name[ind], " ", 1);
            i = i+2;
            ind++;
        }
        else
        {
            memcpy(&real_name[ind], &f_name[i], 1);
            ind++;
        }
    }
    
    ///printf("###\n%s\n###\n", real_name);
    
    /* Open the file with file name */
    FILE* fp = fopen(real_name, "r");
    if (fp == NULL)                         /* 404 not found */
    {
        //printf("404\n###\n");
        error404(fd);
        return;
    }
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* Get file content */
    char content[file_size+1];
    content[file_size] = '\0';
    if (fread(content, file_size, 1, fp) < 0)
    {
        fprintf(stderr, "Error in fread().\n");
        exit(1);
    }
    fclose(fp);
    //printf("###\n%s\n###\n", content);
    
    /* Get content length */
    //printf("###\n%ld\n###\n", file_size);
    char length[32];
    sprintf(length, "%ld\r\n", file_size);
    char content_length[32] = "Content-Length: ";
    strcat(content_length, length);
    
    /* Get content type */
    char* content_type="Content-Type: application/octet-stream\r\n";
    //printf("###\n%s\n###\n", real_name);
    if (strstr(real_name, ".") == NULL)
    {
        content_type  = "Content-Type: application/octet-stream\r\n";
    }
    else if (strstr(real_name, ".jpg")!=NULL)
    {
        content_type = "Content-Type: image/jpg\r\n";
    }
    else if (strstr(real_name, ".png")!=NULL)
    {
        content_type = "Content-Type: image/png\r\n";
    }
    else if (strstr(real_name, ".html") != NULL)
    {
        //printf("###\nHTML caught\n###");
        content_type = "Content-Type: text/html\r\n";
    }
    else if (strstr(real_name, ".txt")!=NULL)
    {
        content_type = "Content-Type: text/txt\r\n";
    }
    
    /* Get status */
    char* status = "HTTP/1.1 200 OK\r\n";
    
    /* Get server */
    char* server = "Server: webserver.c\r\n";
    
    /* Get connection */
    char* connection = "Connection: close\r\n";
    
    /* Get ending signal */
    char* ending = "\r\n\0";
    
    /* Send */
    send(fd, status, strlen(status), 0);
    send (fd, server, strlen(server), 0);
    send(fd, content_type, strlen(content_type), 0);
    send(fd, content_length, strlen(content_length), 0);
    send(fd, connection, strlen(connection), 0);
    send(fd, ending, strlen(ending), 0);
    send(fd, content, file_size, 0);
    
    printf("* Response:\n%s%s%s%s%s", status, server, content_type, content_length, connection);
}

void error404(int fd)
{
    char* err404 = "HTTP/1.1 404 Not Found\r\nServer: webserver.c\r\n\r\n<html><head><title>404 Not Found</head></title><body><p>404 Not Found: No such file exists</p></body></html>";
    send(fd, err404, strlen(err404), 0);
    close(fd);
}
