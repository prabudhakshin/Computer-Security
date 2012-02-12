#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, portno;

	int i, index = 0;
	int samplecount = 0;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    server = gethostbyname(argv[1]);
    portno = atoi(argv[2]);

    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    int wnd_size = 1024;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &wnd_size, sizeof(wnd_size));

    if (sockfd < 0) 
        error("ERROR opening socket");

    //char* req = "GET /welcome.php?user=hello HTTP/1.1\r\n"
    char* req = "GET /photo.jpg HTTP/1.1\r\n"
                "HOST: localhost\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: Mozilla/5.0 (X11; Linux i686) AppleWebKit/535.2 (KHTML, like Gecko) Ubuntu/11.10 Chromium/15.0.874.106 Chrome/15.0.874.106 Safari/535.2\r\n"
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                "Referer: http://localhost/index.php\r\n"
                "Accept-Encoding: gzip,deflate,sdch\r\n"
                "Accept-Language: en-US,en;q=0.8\r\n"
                "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3\r\n"
                "\r\n";
    printf("%s", req);
                 
    printf("waiting for server....\n");
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    printf("Connected to server");
    char recBuf[64000];
    int j = 1;
    send(sockfd, req, strlen(req), 0);
    //while(j++ <= 10) {
    while(1) {
        sleep(1);
        //printf("Here\n");
        j = recv(sockfd, recBuf, 64000, 0); 
        if (j==0)
            break;
        printf("Received %d bytes\n", j);
    }
    sleep(10);
    close(sockfd);

    return 0;
}
